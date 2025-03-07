#include <vector>
#include <numeric>

////////////////////
//// FRAMEWORK HEADERS
#include "FWCore/Framework/interface/global/EDProducer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Utilities/interface/InputTag.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"

#include "DataFormats/L1TParticleFlow/interface/PFCandidate.h"
#include "DataFormats/L1TParticleFlow/interface/PFJet.h"

#include "DataFormats/L1Trigger/interface/EtSum.h"
#include "DataFormats/Math/interface/LorentzVector.h"

// bitwise emulation headers
#include "L1Trigger/Phase2L1ParticleFlow/interface/jetmet/L1PFHtEmulator.h"

class L1MhtPfProducer : public edm::global::EDProducer<> {
public:
  explicit L1MhtPfProducer(const edm::ParameterSet&);
  ~L1MhtPfProducer() override;

private:
  void produce(edm::StreamID, edm::Event& iEvent, const edm::EventSetup& iSetup) const override;
  edm::EDGetTokenT<std::vector<l1t::PFJet>> jetsToken;
  float minJetPt;
  float maxJetEta;

  std::vector<l1ct::Jet> convertEDMToHW(std::vector<l1t::PFJet> edmJets) const;
  std::vector<l1t::EtSum> convertHWToEDM(l1ct::Sum hwSums) const;
};

L1MhtPfProducer::L1MhtPfProducer(const edm::ParameterSet& cfg)
    : jetsToken(consumes<std::vector<l1t::PFJet>>(cfg.getParameter<edm::InputTag>("jets"))),
      minJetPt(cfg.getParameter<double>("minJetPt")),
      maxJetEta(cfg.getParameter<double>("maxJetEta")) {
  produces<std::vector<l1t::EtSum>>();
}

void L1MhtPfProducer::produce(edm::StreamID, edm::Event& iEvent, const edm::EventSetup& iSetup) const {
  // Get the jets from the event
  l1t::PFJetCollection edmJets = iEvent.get(jetsToken);

  // Apply pT and eta selections
  l1t::PFJetCollection edmJetsFiltered;
  std::copy_if(edmJets.begin(), edmJets.end(), std::back_inserter(edmJetsFiltered), [&](auto jet) {
    return jet.pt() > minJetPt && std::abs(jet.eta()) < maxJetEta;
  });

  // Run the emulation
  std::vector<l1ct::Jet> hwJets = convertEDMToHW(edmJetsFiltered);  // convert to the emulator format
  l1ct::Sum hwSums = htmht(hwJets);                                 // call the emulator
  std::vector<l1t::EtSum> edmSums = convertHWToEDM(hwSums);         // convert back to edm format

  // Put the sums in the event
  std::unique_ptr<std::vector<l1t::EtSum>> mhtCollection(new std::vector<l1t::EtSum>(0));
  mhtCollection->push_back(edmSums.at(0));  // HT
  mhtCollection->push_back(edmSums.at(1));  // MHT

  iEvent.put(std::move(mhtCollection));
}

std::vector<l1ct::Jet> L1MhtPfProducer::convertEDMToHW(std::vector<l1t::PFJet> edmJets) const {
  std::vector<l1ct::Jet> hwJets;
  std::for_each(edmJets.begin(), edmJets.end(), [&](l1t::PFJet jet) {
    l1ct::Jet hwJet = l1ct::Jet::unpack(jet.encodedJet());
    hwJets.push_back(hwJet);
  });
  return hwJets;
}

std::vector<l1t::EtSum> L1MhtPfProducer::convertHWToEDM(l1ct::Sum hwSums) const {
  std::vector<l1t::EtSum> edmSums;

  reco::Candidate::PolarLorentzVector htVector;
  htVector.SetPt(hwSums.hwSumPt.to_double());
  htVector.SetPhi(0);
  htVector.SetEta(0);

  reco::Candidate::PolarLorentzVector mhtVector;
  mhtVector.SetPt(hwSums.hwPt.to_double());
  mhtVector.SetPhi(l1ct::Scales::floatPhi(hwSums.hwPhi));
  mhtVector.SetEta(0);

  l1t::EtSum ht(htVector, l1t::EtSum::EtSumType::kTotalHt);
  l1t::EtSum mht(mhtVector, l1t::EtSum::EtSumType::kMissingHt);

  edmSums.push_back(ht);
  edmSums.push_back(mht);
  return edmSums;
}

L1MhtPfProducer::~L1MhtPfProducer() {}

#include "FWCore/Framework/interface/MakerMacros.h"
DEFINE_FWK_MODULE(L1MhtPfProducer);
