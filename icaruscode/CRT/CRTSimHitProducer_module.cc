// sbndcode includes
#include "icaruscode/CRT/CRTProducts/CRTData.hh"
#include "icaruscode/CRT/CRTProducts/CRTHit.hh"
#include "icaruscode/CRT/CRTUtils/CRTHitRecoAlg.h"

// Framework includes
#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Event.h" 
#include "canvas/Persistency/Common/Ptr.h" 
#include "canvas/Persistency/Common/PtrVector.h" 
#include "art/Framework/Principal/Run.h"
#include "art/Framework/Principal/SubRun.h"
#include "art_root_io/TFileService.h" 
#include "art_root_io/TFileDirectory.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "canvas/Persistency/Common/FindManyP.h"
#include "art/Persistency/Common/PtrMaker.h"
#include "canvas/Utilities/InputTag.h"
#include "fhiclcpp/ParameterSet.h"
#include "messagefacility/MessageLogger/MessageLogger.h"
#include <memory>
#include <iostream>
#include <map>
#include <iterator>
#include <algorithm>

// LArSoft
#include "lardataobj/Simulation/SimChannel.h"
#include "lardataobj/Simulation/AuxDetSimChannel.h"
#include "larcore/Geometry/Geometry.h"
#include "larcore/Geometry/AuxDetGeometry.h"
#include "larcorealg/Geometry/GeometryCore.h"
#include "larcorealg/Geometry/PlaneGeo.h"
#include "larcorealg/Geometry/WireGeo.h"
#include "lardataobj/RecoBase/OpFlash.h"
#include "lardata/Utilities/AssociationUtil.h"
#include "lardata/DetectorInfoServices/LArPropertiesService.h"
#include "lardata/DetectorInfoServices/DetectorPropertiesService.h"
#include "lardata/DetectorInfoServices/DetectorClocksService.h"
#include "lardataobj/RawData/ExternalTrigger.h"
#include "larcoreobj/SimpleTypesAndConstants/PhysicalConstants.h"
#include "larcoreobj/SimpleTypesAndConstants/geo_types.h"

// ROOT
#include "TTree.h"
#include "TFile.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TVector3.h"
#include "TGeoManager.h"

namespace icarus {

  class CRTSimHitProducer : public art::EDProducer {
  public:
 
    explicit CRTSimHitProducer(fhicl::ParameterSet const & p);

    // The destructor generated by the compiler is fine for classes
    // without bare pointers or other resource use.

    // Plugins should not be copied or assigned.
    CRTSimHitProducer(CRTSimHitProducer const &) = delete;
    CRTSimHitProducer(CRTSimHitProducer &&) = delete;
    CRTSimHitProducer & operator = (CRTSimHitProducer const &) = delete; 
    CRTSimHitProducer & operator = (CRTSimHitProducer &&) = delete;

    // Required functions.
    void produce(art::Event & e) override;

    // Selected optional functions.
    void beginJob() override;

    void endJob() override;

    void reconfigure(fhicl::ParameterSet const & p);

  private:

    // Params from fcl file.......
    art::InputTag fCrtModuleLabel;      ///< name of crt producer
   
    CRTHitRecoAlg hitAlg;
  }; // class CRTSimHitProducer

  CRTSimHitProducer::CRTSimHitProducer(fhicl::ParameterSet const & p)
  : EDProducer{p}, hitAlg(p.get<fhicl::ParameterSet>("HitAlg"))
  // Initialize member data here, if know don't want to reconfigure on the fly
  {
 
   // Call appropriate produces<>() functions here.
    produces< std::vector<crt::CRTHit> >();
    produces< art::Assns<crt::CRTHit , crt::CRTData> >();
    
    reconfigure(p);

  } // CRTSimHitProducer()

  void CRTSimHitProducer::reconfigure(fhicl::ParameterSet const & p)
  {
    fCrtModuleLabel = (p.get<art::InputTag> ("CrtModuleLabel")); 
  } // CRTSimHitProducer::reconfigure()

  void CRTSimHitProducer::beginJob()
  {

  } // CRTSimHitProducer::beginJob()
 
  void CRTSimHitProducer::produce(art::Event & event)
  {

    std::unique_ptr< std::vector<crt::CRTHit> > CRTHitcol( new std::vector<crt::CRTHit>);
    std::unique_ptr< art::Assns<crt::CRTHit, crt::CRTData> > Hitassn( new art::Assns<crt::CRTHit, crt::CRTData>);
    art::PtrMaker<crt::CRTHit> makeHitPtr(event);

    int nHits = 0;

    // Retrieve list of CRT hits
    art::Handle< std::vector<crt::CRTData>> crtListHandle;
    std::vector<art::Ptr<crt::CRTData> > crtList;

    if (event.getByLabel(fCrtModuleLabel, crtListHandle))
      art::fill_ptr_vector(crtList, crtListHandle);

    mf::LogInfo("CRTSimHitProducer")
      <<"Number of SiPM hits = "<<crtList.size();

    std::vector<std::pair<crt::CRTHit, std::vector<int>>> crtHitPairs = hitAlg.CreateCRTHits(crtList);

    for(auto const& crtHitPair : crtHitPairs){

      CRTHitcol->push_back(crtHitPair.first);
      art::Ptr<crt::CRTHit> hitPtr = makeHitPtr(CRTHitcol->size()-1);
      nHits++;

      for(auto const& data_i : crtHitPair.second){

        Hitassn->addSingle(hitPtr, crtList[data_i]);
      }
    }
      
    event.put(std::move(CRTHitcol));
    event.put(std::move(Hitassn));

    mf::LogInfo("CRTSimHitProducer")
      <<"Number of CRT hits produced = "<<nHits;

  } // CRTSimHitProducer::produce()

  void CRTSimHitProducer::endJob()
  {

  } // CRTSimHitProducer::endJob()

  DEFINE_ART_MODULE(CRTSimHitProducer)

} //end namespace
