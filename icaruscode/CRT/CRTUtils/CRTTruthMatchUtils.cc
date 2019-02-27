
#include "CRTTruthMatchUtils.h"

namespace icarus {

  std::vector<int> CRTTruthMatchUtils::AllTrueIds(art::Handle<std::vector<icarus::crt::CRTHit>> hitHandle, 
                                                   const art::Event& event, art::InputTag hitLabel, int hit_i) {
    std::vector<int> ids;

    art::FindManyP<icarus::crt::CRTData> findManyData(hitHandle, event, hitLabel);
    std::vector<art::Ptr<icarus::crt::CRTData>> data = findManyData.at(hit_i);

    for(auto const& dat : data){
      for(auto const& chan : dat->ChanData()){
        for(auto const& trkid : chan.TrackID()){
          ids.push_back(trkid);
        }
      }
    }

    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
 
    return ids;
  }

  std::vector<int> CRTTruthMatchUtils::AllTrueIds(art::Handle<std::vector<icarus::crt::CRTData>> dataHandle, 
                                                const art::Event& event, art::InputTag dataLabel, int data_i) {

    std::vector<int> ids;

    art::FindManyP<sim::AuxDetIDE> findManyIdes(dataHandle, event, dataLabel);
    std::vector<art::Ptr<sim::AuxDetIDE>> ides = findManyIdes.at(data_i);

    for(size_t i = 0; i < ides.size(); i++){
      ids.push_back(ides[i]->trackID);
    }

    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());

    return ids;
  }

  std::vector<int> CRTTruthMatchUtils::AllTrueIds(art::Handle<std::vector<icarus::crt::CRTHit>> hitHandle, 
                           const art::Event& event, art::InputTag hitLabel, art::InputTag dataLabel, int hit_i){

    std::vector<int> ids;

    art::FindManyP<icarus::crt::CRTData> findManyData(hitHandle, event, hitLabel);
    std::vector<art::Ptr<icarus::crt::CRTData>> data = findManyData.at(hit_i);
    art::FindManyP<sim::AuxDetIDE> findManyIdes(data, event, dataLabel);

    for(size_t i = 0; i < data.size(); i++){
      std::vector<art::Ptr<sim::AuxDetIDE>> ides = findManyIdes.at(i);
      for(size_t j = 0; j < ides.size(); j++){
        ids.push_back(ides[j]->trackID);
      }
    }

    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());

    return ids;
  }

  int CRTTruthMatchUtils::TrueIdFromTotalEnergy(art::Handle<std::vector<icarus::crt::CRTData>> dataHandle, 
                                                const art::Event& event, art::InputTag dataLabel, int data_i){

    std::map<int, double> ids;

    art::FindManyP<sim::AuxDetIDE> findManyIdes(dataHandle, event, dataLabel);
    std::vector<art::Ptr<sim::AuxDetIDE>> ides = findManyIdes.at(data_i);

    for(size_t i = 0; i < ides.size(); i++){
      ids[ides[i]->trackID] += ides[i]->energyDeposited;
    }

    double maxEnergy = -1;
    int trueId = -99999;

    for(auto &id : ids){
      if(id.second > maxEnergy){
        maxEnergy = id.second;
        trueId = id.first;
      }
    }

    return trueId;
  }

  int CRTTruthMatchUtils::TrueIdFromTotalEnergy(art::Handle<std::vector<icarus::crt::CRTHit>> hitHandle, 
                           const art::Event& event, art::InputTag hitLabel, art::InputTag dataLabel, int hit_i){

    std::map<int, double> ids;

    art::FindManyP<icarus::crt::CRTData> findManyData(hitHandle, event, hitLabel);
    std::vector<art::Ptr<icarus::crt::CRTData>> data = findManyData.at(hit_i);
    art::FindManyP<sim::AuxDetIDE> findManyIdes(data, event, dataLabel);

    for(size_t i = 0; i < data.size(); i++){
      std::vector<art::Ptr<sim::AuxDetIDE>> ides = findManyIdes.at(i);
      for(size_t j = 0; j < ides.size(); j++){
        ids[ides[j]->trackID] += ides[j]->energyDeposited;
      }
    }

    double maxEnergy = -1;
    int trueId = -99999;

    for(auto &id : ids){
      if(id.second > maxEnergy){
        maxEnergy = id.second;
        trueId = id.first;
      }
    }

    return trueId;
  }

} //namespace icarus
