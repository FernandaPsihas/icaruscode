////////////////////////////////////////////////////////////////////////
/// \file   Response.cc
/// \author T. Usher
////////////////////////////////////////////////////////////////////////

#include <cmath>
#include "shprIResponse.h"
#include "art/Utilities/ToolMacros.h"
#include "art_root_io/TFileService.h"
#include "messagefacility/MessageLogger/MessageLogger.h"
#include "cetlib_except/exception.h"
#include "larcore/CoreUtils/ServiceUtil.h"
#include "larcoreobj/SimpleTypesAndConstants/geo_types.h"
#include "lardata/Utilities/SignalShaper.h"
//#include "lardata/Utilities/LArFFT.h"
#include "lardata/DetectorInfoServices/DetectorPropertiesService.h"
#include "larcore/Geometry/Geometry.h"

#include "art/Utilities/make_tool.h"
#include "icaruscode/Utilities/SignalShaperServiceICARUS.h"
#include "icaruscode/Utilities/tools/IWaveformTool.h"
#include "IFieldResponse.h"
#include "IElectronicsResponse.h"
#include "shprIFilter.h"

#include "TProfile.h"

#include <Eigen/Core>
#include <unsupported/Eigen/FFT>

#include <fstream>
#include <iomanip>

namespace icarus_tool
{

class shprResponse : shprIResponse
{
public:
    explicit shprResponse(const fhicl::ParameterSet& pset);
    
    ~shprResponse() {}
    
    void configure(const fhicl::ParameterSet& pset)   override;
    void setResponse(double weight)                   override;
    void outputHistograms(art::TFileDirectory&) const override;
    
    size_t                      getPlane()               const override {return fThisPlane;}
    
    const IFieldResponse*       getFieldResponse()       const override {return fFieldResponse.get();}
    const IElectronicsResponse* getElectronicsResponse() const override {return fElectronicsResponse.get();}
    const shprIFilter*            getFilter()              const override {return fFilter.get();}
    
    const util::SignalShaper&   getSignalShaper()        const override {return fSignalShaper;}
    
private:
    using IFieldResponsePtr       = std::unique_ptr<icarus_tool::IFieldResponse>;
    using IElectronicsResponsePtr = std::unique_ptr<icarus_tool::IElectronicsResponse>;
    using IFilterPtr              = std::unique_ptr<icarus_tool::shprIFilter>;

    // Utility routine for converting numbers to strings
    std::string             numberToString(int number);
    
    int fFFTSize;		//size of transform
    std::string fFFTOption;     //FFTW setting

    // Member variables from the fhicl file
    size_t                  fThisPlane;
    double                  f3DCorrection;
    double                  fTimeScaleFactor;
    int                     fDeconvPol;
    
    // Keep track of our base tools
    IFieldResponsePtr       fFieldResponse;
    IElectronicsResponsePtr fElectronicsResponse;
    IFilterPtr              fFilter;
    
    // The actual response function
    util::SignalShaper      fSignalShaper;
};
    
//----------------------------------------------------------------------
// Constructor.
shprResponse::shprResponse(const fhicl::ParameterSet& pset)
  : fFFTSize   (pset.get< int >("FFTSize"))
  , fFFTOption (pset.get< std::string >("FFTOption"))
  , fSignalShaper(fFFTSize,fFFTOption)
{
    configure(pset);
}
    
void shprResponse::configure(const fhicl::ParameterSet& pset)
{
    // Start by recovering the parameters
    fThisPlane       = pset.get<size_t>("Plane");
    f3DCorrection    = pset.get<size_t>("Correction3D");
    fTimeScaleFactor = pset.get<size_t>("TimeScaleFactor");
    fDeconvPol       = pset.get<int   >("DeconvPol");
    //fFFTSize         = pset.get<int   >("FFTSize");
    
    // Build out the underlying tools we'll be using
    fFieldResponse       = art::make_tool<icarus_tool::IFieldResponse>(pset.get<fhicl::ParameterSet>("FieldResponse"));
    fElectronicsResponse = art::make_tool<icarus_tool::IElectronicsResponse>(pset.get<fhicl::ParameterSet>("ElectronicsResponse"));
    fFilter              = art::make_tool<icarus_tool::shprIFilter>(pset.get<fhicl::ParameterSet>("Filter"));
    
    return;
}
    
void shprResponse::setResponse(double weight)
{
    // We'll need the FFT service
    //art::ServiceHandle<util::LArFFT> fastFourierTransform;
    
    auto const* detprop = lar::providerFrom<detinfo::DetectorPropertiesService>();

    // First of all set the field response
    fFieldResponse->setResponse(weight, f3DCorrection, fTimeScaleFactor);

    // handle the electronics response for this plane
    fElectronicsResponse->setResponse(fFieldResponse->getResponseVec().size(), fFieldResponse->getBinWidth());
    
    // Next we perform the convolution of the field and electronics responses and then invert to get the
    // interim response function which will be set up in the time binning/range of the field response
    const std::vector<std::complex<double>>& fieldResponseFFTVec       = fFieldResponse->getResponseFFTVec();
    const std::vector<std::complex<double>>& electronicsResponseFFTVec = fElectronicsResponse->getResponseFFTVec();
    
    std::vector<std::complex<double>> curResponseFFTVec(fieldResponseFFTVec.size());
    
    std::transform(fieldResponseFFTVec.begin(), fieldResponseFFTVec.begin() + fieldResponseFFTVec.size()/2, electronicsResponseFFTVec.begin(), curResponseFFTVec.begin(), std::multiplies<std::complex<double>>());
    
    // And now we recover the current response vector which is the convolution of the two above
    // (and still in the units of the original field response)
    std::vector<double> curResponseVec(fieldResponseFFTVec.size());
    
    Eigen::FFT<double> eigenFFT;

    eigenFFT.inv(curResponseVec, curResponseFFTVec);

    // Now set to the task of determing the actual sampling response
    // We have to remember that the bin size for determining the field response probably
    // does not match that for the detector readout so we'll need to "convert"
    // from one to the other.
    //size_t fftSize = fastFourierTransform->FFTSize();
    
    std::vector<double> samplingTimeVec( fFFTSize, 0. );
    
    // Recover the combined response from above
//    const std::vector<double>& curResponseVec = fSignalShaper.Response_save();
    
    double respIntegral = std::accumulate(curResponseVec.begin(),curResponseVec.end(),0.);
    
    mf::LogInfo("shprResponse_tool") << "***** Response for plane: " << fThisPlane << " ******" << "\n"
                                 << "      initial response integral: " << respIntegral << std::endl;
    
    // Need two factors: 1) the detector sampling rate and 2) the response sampling rate
    double samplingRate = detprop->SamplingRate() * 1.e-3;                // We want this in us/bin
    double responseRate = fFieldResponse->getBinWidth() * 1.e-3;          // We want this in us/bin
    double rateRatio    = samplingRate / responseRate;                    // This gives the ratio of time bins for full readout to response bins
    
    // The idea is to step through each bin of the sampling response vector and then to
    // look up the corresponding bins in the current response vector. Since the two sample
    // rates are not the same there will be some "stretching" between the two. In addition,
    // we want to continue to allow for the possibility for further sample stretching
    double binScaleFactor = rateRatio * f3DCorrection * fTimeScaleFactor;
    
    // ok, do the loop
    for(size_t sampleIdx = 0; sampleIdx < samplingTimeVec.size(); sampleIdx++)
    {
        // calculate the index for the response
        size_t responseLowIdx = std::floor(sampleIdx * binScaleFactor);
        
        if (responseLowIdx < curResponseVec.size())
        {
            // Calculate the index for the next bin
            size_t responseHiIdx = std::floor((sampleIdx + 1) * binScaleFactor);
            
            // This can't happen? But protect against zero divides...
            if (responseHiIdx == responseLowIdx) responseHiIdx += 1;
            
            if (responseHiIdx >= curResponseVec.size()) break;
            
            std::vector<double>::const_iterator curResponseItr = curResponseVec.begin();
            
            std::advance(curResponseItr,responseLowIdx);
            
            int nBins = responseHiIdx - responseLowIdx;
            
            // Obtain the average of these bins
            double aveResponse = std::accumulate(curResponseItr,curResponseItr+nBins,0.)/double(nBins);
            
            // Now interpolate between the two bins to get the sampling response for this bin
//            double responseSlope = (curResponseVec.at(responseHiIdx) - curResponseVec.at(responseLowIdx)) / (responseHiIdx - responseLowIdx);
//            double response      = curResponseVec.at(responseLowIdx) + 0.5 * responseSlope * (responseHiIdx - responseLowIdx);

            samplingTimeVec[sampleIdx] = aveResponse;
        }
    }
    
    // We need to scale by the binScaleFactor to preserve normalization
    std::transform(samplingTimeVec.begin(),samplingTimeVec.end(),samplingTimeVec.begin(),std::bind(std::multiplies<double>(),std::placeholders::_1,binScaleFactor));
    
    respIntegral = std::accumulate(samplingTimeVec.begin(),samplingTimeVec.end(),0.);
    
    mf::LogInfo("shprResponse_tool")  << "      final response integral: " << respIntegral << std::endl;

    fSignalShaper.Reset();
    fSignalShaper.AddResponseFunction( samplingTimeVec, true);

    // Set up the filter
    fFilter->setResponse(fFFTSize, f3DCorrection, fTimeScaleFactor);
    
    // Finalize the Signal Shaping
    fSignalShaper.AddFilterFunction(fFilter->getResponseVec());
    fSignalShaper.SetDeconvKernelPolarity( fDeconvPol );         // Note that this is only important if set_normflag above has been set to true
    fSignalShaper.set_normflag(false);                           // WE are handling the normalization
    fSignalShaper.CalculateDeconvKernel();

    return;
}
    
void shprResponse::outputHistograms(art::TFileDirectory& histDir) const
{
    // Create a subfolder in which to place the "response" histograms
    std::string thisResponse = "ResponsesPlane_" + std::to_string(fThisPlane);
    
    art::TFileDirectory dir = histDir.mkdir(thisResponse.c_str());
    
    // Do the field response histograms
    fFieldResponse->outputHistograms(dir);
    fElectronicsResponse->outputHistograms(dir);
    fFilter->outputHistograms(dir);
    
    // Now make hists for the full response
    std::string dirName = "Response_" + std::to_string(fThisPlane);
    
    art::TFileDirectory        responesDir  = dir.mkdir(dirName.c_str());
    const std::vector<double>& responseVec  = this->getSignalShaper().Response();
    auto const*                detprop      = lar::providerFrom<detinfo::DetectorPropertiesService>();
    int                        numBins      = responseVec.size();
    double                     samplingRate = detprop->SamplingRate(); // **Sampling time in ns**
    double                     maxFreq      = 1.e6 / (2. * samplingRate);
    double                     minFreq      = 1.e6 / (2. * samplingRate * double(numBins));
    std::string                histName     = "Response_Plane_" + std::to_string(fThisPlane);
    TProfile*                  hist         = dir.make<TProfile>(histName.c_str(), "Response;Time(us)", numBins, 0., numBins * samplingRate * 1.e-3);
    
    for(int bin = 0; bin < numBins; bin++)
    {
        hist->Fill((double(bin) + 0.5) * samplingRate * 1.e-3, responseVec.at(bin), 1.);
    }

    // Get the FFT, need the waveform tool for consistency
    fhicl::ParameterSet waveformToolParams;
    
    waveformToolParams.put<std::string>("tool_type","Waveform");
    
    std::unique_ptr<icarus_tool::IWaveformTool> waveformTool = art::make_tool<icarus_tool::IWaveformTool>(waveformToolParams);
    
    std::vector<double> powerVec;
    
    waveformTool->getFFTPower(responseVec, powerVec);
    
    double      freqWidth = maxFreq / (powerVec.size() - 1);
    std::string freqName  = "Response_FFTPlane_" + std::to_string(fThisPlane);
    TProfile*   freqHist  = dir.make<TProfile>(freqName.c_str(), "Response;Frequency(MHz)", powerVec.size(), minFreq, maxFreq);
    
    for(size_t idx = 0; idx < powerVec.size(); idx++)
    {
        double freq = freqWidth * (idx + 0.5);
        
        freqHist->Fill(freq, powerVec.at(idx), 1.);
    }
    
    const std::vector<std::complex<double>>& convKernel = this->getSignalShaper().ConvKernel();
    
    std::string convKernelName   = "ConvKernel_" + std::to_string(fThisPlane);
    TProfile*   fullResponseHist = dir.make<TProfile>(convKernelName.c_str(), "Convolution Kernel;Frequency(MHz)", convKernel.size(), minFreq, maxFreq);

    for(size_t idx = 0; idx < convKernel.size(); idx++)
    {
        double freq = freqWidth * (idx + 0.5);
        
        fullResponseHist->Fill(freq, std::abs(convKernel.at(idx)), 1.);
    }
    
    const std::vector<std::complex<double>>& deconKernel = this->getSignalShaper().DeconvKernel();
    
    std::string deconName = "DeconKernel_" + std::to_string(fThisPlane);
    TProfile*   deconHist = dir.make<TProfile>(deconName.c_str(), "Deconvolution Kernel;Frequency(MHz)", deconKernel.size(), minFreq, maxFreq);
    
    for(size_t idx = 0; idx < deconKernel.size(); idx++)
    {
        double freq = freqWidth * (idx + 0.5);
        
        deconHist->Fill(freq, std::abs(deconKernel.at(idx)), 1.);
    }

    return;
}

std::string shprResponse::numberToString(int number)
{
    std::ostringstream string;
    
    string << std::setfill('0') << std::setw(2) << number;
    
    return string.str();
}

    
DEFINE_ART_CLASS_TOOL(shprResponse)
}