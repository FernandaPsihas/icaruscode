////////////////////////////////////////////////////////////////////////
// Class:       wvfAnaicarus
// Plugin Type: analyzer (art v2_09_06)
// File:        wvfAnaicarus_module.cc
//
// Generated at Wed Feb  7 17:51:39 2018 by Andrea Falcone using cetskelgen
// from cetlib version v3_01_03.
//
// Analyzer to read optical waveforms
//
// Authors: L. Paulucci and F. Marinho
////////////////////////////////////////////////////////////////////////

#include <vector>
#include <cmath>
#include <memory>
#include <string>

#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h"
#include "art/Framework/Principal/SubRun.h"
#include "art_root_io/TFileService.h"
#include "canvas/Utilities/InputTag.h"
#include "fhiclcpp/ParameterSet.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include "lardataobj/RawData/OpDetWaveform.h"
#include "lardataobj/Simulation/sim.h"
#include "lardataobj/Simulation/SimChannel.h"
#include "lardataobj/Simulation/SimPhotons.h"

#include "TH1D.h"
#include "TFile.h"
#include "TTree.h"

namespace opdet{

class wvfAnaicarus;


class wvfAnaicarus : public art::EDAnalyzer {
public:
  explicit wvfAnaicarus(fhicl::ParameterSet const & p);
  // The compiler-generated destructor is fine for non-base
  // classes without bare pointers or other resource use.

  // Plugins should not be copied or assigned.
  wvfAnaicarus(wvfAnaicarus const &) = delete;
  wvfAnaicarus(wvfAnaicarus &&) = delete;
  wvfAnaicarus & operator = (wvfAnaicarus const &) = delete;
  wvfAnaicarus & operator = (wvfAnaicarus &&) = delete;

  // Required functions.
  void analyze(art::Event const & e) override;

  //Selected optional functions
  void beginJob() override;
  void endJob() override;

private:

  size_t fEvNumber;
  size_t fChNumber;
  double fSampling;
 // double fAmp;
  double fStartTime;
  double fEndTime;

  // Declare member data here.
  std::string fInputModuleName;
  std::stringstream histname;

};


wvfAnaicarus::wvfAnaicarus(fhicl::ParameterSet const & p)
  :
  EDAnalyzer(p)  // ,
 // More initializers here.
{
  fInputModuleName = p.get< std::string >("InputModule" );
  fSampling = p.get< double >("Sampling" );

}

void wvfAnaicarus::beginJob(){

//  art::ServiceHandle<art::TFileService> tfs;
//  fWaveformTree = tfs->make<TTree>("WaveformTree", "event by event info");
//  fWaveformTree->Branch("event",&fEvNumber,"fEvNumber/I");
//  fWaveformTree->Branch("ch",&fChNumber,"fChNumber/I");
//  fWaveformTree->Branch("waveform",&fAmp,"fAmp/D");
//  fWaveformTree->Branch("time",&fTime,"fTime/D");
}


void wvfAnaicarus::analyze(art::Event const & e)
{
  // Implementation of required member function here.
  std::cout << "My module on event #" << e.id().event() << std::endl;

  art::ServiceHandle<art::TFileService> tfs;
  fEvNumber = e.id().event();
  
  art::Handle< std::vector< raw::OpDetWaveform > > waveHandle;
  e.getByLabel(fInputModuleName, waveHandle);

  if(!waveHandle.isValid()){
	std::cout << Form("Did not find any G4 photons from a producer: %s", "largeant") << std::endl;
  }

  std::cout << "Number of photon channel: " <<waveHandle->size() << std::endl;
//  unsigned int nChannels = pmtHandle->size();

  for(auto const& wvf : (*waveHandle)){
	fChNumber = wvf.ChannelNumber();
  	std::cout << "Photon channel: " << fChNumber << std::endl;
//	std::stringstream histName;
	histname.str(std::string());
	histname << "event_" << fEvNumber 
      		 << "_opchannel_" <<fChNumber;
	
	fStartTime = wvf.TimeStamp()/1000; //in us
	fEndTime = double(wvf.size())/fSampling + fStartTime;
	fEndTime = fEndTime/1000; //in us

	//Create a new histogram
	TH1D *wvfHist = tfs->make< TH1D >(histname.str().c_str(), TString::Format(";t - %f (#mus);",fStartTime), wvf.size(), fStartTime, fEndTime);
	//if(fChNumber==0){
//	  std::copy(wvf.begin(),wvf.end(),fAmp);
 	for(unsigned int i=0; i<wvf.size();i++){
	  wvfHist->SetBinContent(i+1,(double)wvf[i]);
//	  fAmp = (double)wvf[i];
	//  std::cout << fAmp << fTime << std::endl;
//	  fWaveformTree->Fill();
	  	//std::cout << wvf[i] << std::endl;
	}	
  }
}


void wvfAnaicarus::endJob(){
}

DEFINE_ART_MODULE(wvfAnaicarus)

}
