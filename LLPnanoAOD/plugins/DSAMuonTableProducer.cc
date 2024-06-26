// system include files
#include <memory>

// user include files
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/global/EDProducer.h"

#include "FWCore/Framework/interface/Event.h"

#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"

#include "DataFormats/PatCandidates/interface/Muon.h"
#include "DataFormats/MuonReco/interface/MuonSelectors.h"
#include "DataFormats/NanoAOD/interface/FlatTable.h"

#include "TrackingTools/TransientTrack/interface/TransientTrackBuilder.h"
#include "TrackingTools/Records/interface/TransientTrackRecord.h"
#include "TrackingTools/IPTools/interface/IPTools.h"
#include "DataFormats/TrackReco/interface/Track.h"
#include "DataFormats/VertexReco/interface/VertexFwd.h"
#include "DataFormats/VertexReco/interface/Vertex.h"


class DSAMuonTableProducer : public edm::global::EDProducer<> {
  public:
    DSAMuonTableProducer(const edm::ParameterSet &iConfig)
      :
      dsaMuonTag_(consumes<std::vector<reco::Track>>(iConfig.getParameter<edm::InputTag>("dsaMuons"))),
      muonTag_(consumes<std::vector<pat::Muon>>(iConfig.getParameter<edm::InputTag>("muons"))),
      vtxTag_(consumes<reco::VertexCollection>(iConfig.getParameter<edm::InputTag>("primaryVertex"))),
      bsTag_(consumes<reco::BeamSpot>(iConfig.getParameter<edm::InputTag>("beamspot")))
      {
      produces<nanoaod::FlatTable>("DSAMuon");
    }

    ~DSAMuonTableProducer() override {}

    static void fillDescriptions(edm::ConfigurationDescriptions & descriptions) {
      edm::ParameterSetDescription desc;
      desc.add<edm::InputTag>("dsaMuons")->setComment("input displaced standalone muon collection");
      desc.add<edm::InputTag>("muons")->setComment("input muon collection");
      desc.add<edm::InputTag>("primaryVertex")->setComment("input primary vertex collection");
      desc.add<edm::InputTag>("beamspot")->setComment("input beamspot collection");
      descriptions.add("DSAMuonTable", desc);
    }

  private:
    void produce(edm::StreamID, edm::Event&, edm::EventSetup const&) const override;

    bool passesDisplacedID(const reco::Track& dsaMuon) const;
    int getMatches(const pat::Muon& muon, const reco::Track& dsaMuon, float minPositionDiff=1e-6) const;
    int getDTMatches(const pat::Muon& muon, const reco::Track& dsaMuon, float minPositionDiff=1e-6) const;
    int getCSCMatches(const pat::Muon& muon, const reco::Track& dsaMuon, float minPositionDiff=1e-6) const;
    int getTotSegments(const reco::Track& dsaMuon) const;
    int getDTSegments(const reco::Track& dsaMuon) const;
    int getCSCSegments(const reco::Track& dsaMuon) const;

    edm::EDGetTokenT<std::vector<reco::Track>> dsaMuonTag_;
    edm::EDGetTokenT<std::vector<pat::Muon>> muonTag_;
    edm::EDGetTokenT<reco::VertexCollection> vtxTag_;
    edm::EDGetTokenT<reco::BeamSpot> bsTag_;

};


void DSAMuonTableProducer::produce(edm::StreamID, edm::Event& iEvent, const edm::EventSetup& iSetup) const {
  
  float minPositionDiffForMatching = 1e-6;

  edm::Handle<std::vector<reco::Track>> dsaMuonHandle;
  iEvent.getByToken(dsaMuonTag_, dsaMuonHandle);
  edm::Handle<std::vector<pat::Muon>> muonHandle;
  iEvent.getByToken(muonTag_, muonHandle);

  edm::Handle<reco::VertexCollection> primaryVertexHandle;
  iEvent.getByToken(vtxTag_, primaryVertexHandle);
  const auto& pv = primaryVertexHandle->at(0);
  GlobalPoint primaryVertex(pv.x(), pv.y(), pv.z());

  edm::Handle<reco::BeamSpot> beamSpotHandle;
  iEvent.getByToken(bsTag_, beamSpotHandle);
  const auto& bs = beamSpotHandle->position();
  GlobalPoint beamSpot(bs.x(), bs.y(), bs.z());
  reco::Vertex beamSpotVertex(beamSpotHandle->position(), beamSpotHandle->covariance3D());

  edm::ESHandle<TransientTrackBuilder> builder;
  iSetup.get<TransientTrackRecord>().get("TransientTrackBuilder", builder);

  unsigned int nDSAMuons = dsaMuonHandle->size();
  unsigned int nMuons = muonHandle->size();

  std::vector<float> idx,pt,ptErr,eta,etaErr,phi,phiErr,charge,dxy,dz,vx,vy,vz,chi2,ndof;

  std::vector<float> trkNumPlanes,trkNumHits,trkNumDTHits,trkNumCSCHits,normChi2,outerEta, outerPhi;

  std::vector<float> dzPV,dzPVErr,dxyPVTraj,dxyPVTrajErr,dxyPVSigned,dxyPVSignedErr,ip3DPVSigned,ip3DPVSignedErr;
  std::vector<float> dxyBS,dxyBSErr,dzBS,dzBSErr,dxyBSTraj,dxyBSTrajErr,dxyBSSigned,dxyBSSignedErr,ip3DBSSigned,ip3DBSSignedErr;

  std::vector<float> displacedId;
  std::vector<std::vector<float>> nMatchesPerMuon;
  std::vector<float> muonMatch1,muonMatch1idx,muonMatch2,muonMatch2idx,muonMatch3,muonMatch3idx,muonMatch4,muonMatch4idx,muonMatch5,muonMatch5idx;
  std::vector<float> muonDTMatch1,muonDTMatch1idx,muonDTMatch2,muonDTMatch2idx,muonDTMatch3,muonDTMatch3idx;
  std::vector<float> muonCSCMatch1,muonCSCMatch1idx,muonCSCMatch2,muonCSCMatch2idx,muonCSCMatch3,muonCSCMatch3idx;
  std::vector<float> nSegments,nDTSegments,nCSCSegments;

  std::vector<float> totPATmatches, totPATmatchesOS, totPATmatchesDisplID, totPATmatchesDisplIDOS, totLoosePATmatchesDisplID, totLoosePATmatchesDisplIDOS;
  std::vector<float> LoosePATmatchesDisplIDOSpt, LoosePATmatchesDisplIDOSdsaDetID1, LoosePATmatchesDisplIDOSdsaDetID2, LoosePATmatchesDisplIDOSdsaDetID3;

  for(unsigned int i = 0; i < nDSAMuons; i++) {

    const reco::Track & dsaMuon = (*dsaMuonHandle)[i];
    idx.push_back(i);

    pt.push_back(dsaMuon.pt());
    ptErr.push_back(dsaMuon.ptError());
    eta.push_back(dsaMuon.eta());
    etaErr.push_back(dsaMuon.etaError());
    phi.push_back(dsaMuon.phi());
    phiErr.push_back(dsaMuon.phiError());
    charge.push_back(dsaMuon.charge());
    dxy.push_back(dsaMuon.dxy());
    dz.push_back(dsaMuon.dz());
    vx.push_back(dsaMuon.vx());
    vy.push_back(dsaMuon.vy());
    vz.push_back(dsaMuon.vz());
    chi2.push_back(dsaMuon.chi2());
    ndof.push_back(dsaMuon.ndof());

    trkNumPlanes.push_back(dsaMuon.hitPattern().muonStationsWithValidHits());
    trkNumHits.push_back(dsaMuon.hitPattern().numberOfValidMuonHits());
    trkNumDTHits.push_back(dsaMuon.hitPattern().numberOfValidMuonDTHits());
    trkNumCSCHits.push_back(dsaMuon.hitPattern().numberOfValidMuonCSCHits());
    normChi2.push_back(dsaMuon.normalizedChi2());

    outerEta.push_back(dsaMuon.outerEta());
    outerPhi.push_back(dsaMuon.outerPhi());

    dzPV.push_back(dsaMuon.dz(pv.position()));
    dzPVErr.push_back(std::hypot(dsaMuon.dzError(), pv.zError()));
    reco::TransientTrack transientTrack = builder->build(dsaMuon);
    TrajectoryStateClosestToPoint trajectoryPV = transientTrack.trajectoryStateClosestToPoint(primaryVertex);
    dxyPVTraj.push_back(trajectoryPV.perigeeParameters().transverseImpactParameter());
    dxyPVTrajErr.push_back(trajectoryPV.perigeeError().transverseImpactParameterError());
    GlobalVector muonRefTrackDir(dsaMuon.px(),dsaMuon.py(),dsaMuon.pz());
    dxyPVSigned.push_back(IPTools::signedTransverseImpactParameter(transientTrack, muonRefTrackDir, pv).second.value());
    dxyPVSignedErr.push_back(IPTools::signedTransverseImpactParameter(transientTrack, muonRefTrackDir, pv).second.error());

    ip3DPVSigned.push_back(IPTools::signedImpactParameter3D(transientTrack, muonRefTrackDir, pv).second.value());
    ip3DPVSignedErr.push_back(IPTools::signedImpactParameter3D(transientTrack, muonRefTrackDir, pv).second.error());  

    dxyBS.push_back(dsaMuon.dxy(bs));
    dxyBSErr.push_back(std::hypot(dsaMuon.dxyError(), beamSpotVertex.zError()));
    dzBS.push_back(dsaMuon.dz(bs));
    dzBSErr.push_back(std::hypot(dsaMuon.dzError(), beamSpotVertex.zError()));
    TrajectoryStateClosestToBeamLine trajectoryBS = transientTrack.stateAtBeamLine();
    dxyBSTraj.push_back(trajectoryBS.transverseImpactParameter().value());
    dxyBSTrajErr.push_back(trajectoryBS.transverseImpactParameter().error());
    dxyBSSigned.push_back(IPTools::signedTransverseImpactParameter(transientTrack, muonRefTrackDir, beamSpotVertex).second.value());
    dxyBSSignedErr.push_back(IPTools::signedTransverseImpactParameter(transientTrack, muonRefTrackDir, beamSpotVertex).second.error());  

    ip3DBSSigned.push_back(IPTools::signedImpactParameter3D(transientTrack, muonRefTrackDir, beamSpotVertex).second.value());
    ip3DBSSignedErr.push_back(IPTools::signedImpactParameter3D(transientTrack, muonRefTrackDir, beamSpotVertex).second.error());

    float passesDisplacedId = 0;
    if(passesDisplacedID(dsaMuon)) passesDisplacedId=1;
    displacedId.push_back(passesDisplacedId);

    // Assigning 5 best matches and corresponding muon indices
    std::vector<std::pair<float, float>> muonMatches(5, std::make_pair(-1.0,-1.0));
    std::vector<std::pair<float, float>> muonDTMatches(5, std::make_pair(-1.0,-1.0));
    std::vector<std::pair<float, float>> muonCSCMatches(5, std::make_pair(-1.0,-1.0));
    std::vector<float> nMuonMatches;
    std::vector<float> nMuonDTMatches;
    std::vector<float> nMuonCSCMatches;
    for (unsigned int j = 0; j < nMuons; j++){
      if (j > 4) break;
      const pat::Muon & muon = (*muonHandle)[j];
      // Muon-DSA Matches Table
      int nMatches = getMatches(muon, dsaMuon, minPositionDiffForMatching);
      int nDTMatches = getDTMatches(muon, dsaMuon, minPositionDiffForMatching);
      int nCSCMatches = getCSCMatches(muon, dsaMuon, minPositionDiffForMatching);
      muonMatches[j] = std::make_pair(nMatches, j);
      muonDTMatches[j] = std::make_pair(nDTMatches, j);
      muonCSCMatches[j] = std::make_pair(nCSCMatches, j);
      nMuonMatches.push_back(nMatches);
      nMuonDTMatches.push_back(nDTMatches);
      nMuonCSCMatches.push_back(nCSCMatches);
    }
    nMatchesPerMuon.push_back(nMuonMatches);
    nMatchesPerMuon.push_back(nMuonDTMatches);
    nMatchesPerMuon.push_back(nMuonCSCMatches);
    std::sort(muonMatches.rbegin(), muonMatches.rend());
    std::sort(muonDTMatches.rbegin(), muonDTMatches.rend());
    std::sort(muonCSCMatches.rbegin(), muonCSCMatches.rend());
    muonMatch1.push_back(muonMatches[0].first);
    muonMatch1idx.push_back(muonMatches[0].second);
    muonMatch2.push_back(muonMatches[1].first);
    muonMatch2idx.push_back(muonMatches[1].second);
    muonMatch3.push_back(muonMatches[2].first);
    muonMatch3idx.push_back(muonMatches[2].second);
    muonMatch4.push_back(muonMatches[3].first);
    muonMatch4idx.push_back(muonMatches[3].second);
    muonMatch5.push_back(muonMatches[4].first);
    muonMatch5idx.push_back(muonMatches[4].second);

    muonDTMatch1.push_back(muonDTMatches[0].first);
    muonDTMatch1idx.push_back(muonDTMatches[0].second);
    muonDTMatch2.push_back(muonDTMatches[1].first);
    muonDTMatch2idx.push_back(muonDTMatches[1].second);
    muonDTMatch3.push_back(muonDTMatches[2].first);
    muonDTMatch3idx.push_back(muonDTMatches[2].second);
    muonCSCMatch1.push_back(muonCSCMatches[0].first);
    muonCSCMatch1idx.push_back(muonCSCMatches[0].second);
    muonCSCMatch2.push_back(muonCSCMatches[1].first);
    muonCSCMatch2idx.push_back(muonCSCMatches[1].second);
    muonCSCMatch3.push_back(muonCSCMatches[2].first);
    muonCSCMatch3idx.push_back(muonCSCMatches[2].second);

    nSegments.push_back(getTotSegments(dsaMuon));
    nDTSegments.push_back(getDTSegments(dsaMuon));
    nCSCSegments.push_back(getCSCSegments(dsaMuon));

  }

  auto dsaMuonTab = std::make_unique<nanoaod::FlatTable>(dsaMuonHandle->size(), "DSAMuon", false, false);

  dsaMuonTab->addColumn<float>("idx", idx, "",  nanoaod::FlatTable::FloatColumn);

  dsaMuonTab->addColumn<float>("pt", pt, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("ptErr", ptErr, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("eta", eta, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("etaErr", etaErr, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("phi", phi, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("phiErr", phiErr, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("charge", charge, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("dxy", dxy, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("dz", dz, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("vx", vx, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("vy", vy, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("vz", vz, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("chi2", chi2, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("ndof", ndof, "",  nanoaod::FlatTable::FloatColumn);

  dsaMuonTab->addColumn<float>("trkNumPlanes", trkNumPlanes, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("trkNumHits", trkNumHits, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("trkNumDTHits", trkNumDTHits, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("trkNumCSCHits", trkNumCSCHits, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("normChi2", normChi2, "",  nanoaod::FlatTable::FloatColumn);

  dsaMuonTab->addColumn<float>("outerEta", outerEta, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("outerPhi", outerPhi, "",  nanoaod::FlatTable::FloatColumn);
  
  dsaMuonTab->addColumn<float>("dzPV", dzPV, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("dzPVErr", dzPVErr, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("dxyPVTraj", dxyPVTraj, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("dxyPVTrajErr", dxyPVTrajErr, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("dxyPVSigned", dxyPVSigned, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("dxyPVSignedErr", dxyPVSignedErr, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("ip3DPVSigned", ip3DPVSigned, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("ip3DPVSignedErr", ip3DPVSignedErr, "",  nanoaod::FlatTable::FloatColumn);

  dsaMuonTab->addColumn<float>("dxyBS", dxyBS, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("dxyBSErr", dxyBSErr, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("dzBS", dzBS, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("dzBSErr", dzBSErr, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("dxyBSTraj", dxyBSTraj, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("dxyBSTrajErr", dxyBSTrajErr, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("dxyBSSigned", dxyBSSigned, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("dxyBSSignedErr", dxyBSSignedErr, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("ip3DBSSigned", ip3DBSSigned, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("ip3DBSSignedErr", ip3DBSSignedErr, "",  nanoaod::FlatTable::FloatColumn);

  dsaMuonTab->addColumn<float>("displacedID", displacedId, "",  nanoaod::FlatTable::FloatColumn);

  dsaMuonTab->addColumn<float>("muonMatch1", muonMatch1, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("muonMatch1idx", muonMatch1idx, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("muonMatch2", muonMatch2, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("muonMatch2idx", muonMatch2idx, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("muonMatch3", muonMatch3, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("muonMatch3idx", muonMatch3idx, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("muonMatch4", muonMatch4, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("muonMatch4idx", muonMatch4idx, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("muonMatch5", muonMatch5, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("muonMatch5idx", muonMatch5idx, "",  nanoaod::FlatTable::FloatColumn);

  dsaMuonTab->addColumn<float>("muonDTMatch1", muonDTMatch1, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("muonDTMatch1idx", muonDTMatch1idx, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("muonDTMatch2", muonDTMatch2, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("muonDTMatch2idx", muonDTMatch2idx, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("muonDTMatch3", muonDTMatch3, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("muonDTMatch3idx", muonDTMatch3idx, "",  nanoaod::FlatTable::FloatColumn);

  dsaMuonTab->addColumn<float>("muonCSCMatch1", muonCSCMatch1, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("muonCSCMatch1idx", muonCSCMatch1idx, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("muonCSCMatch2", muonCSCMatch2, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("muonCSCMatch2idx", muonCSCMatch2idx, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("muonCSCMatch3", muonCSCMatch3, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("muonCSCMatch3idx", muonCSCMatch3idx, "",  nanoaod::FlatTable::FloatColumn);

  dsaMuonTab->addColumn<float>("nSegments", nSegments, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("nDTSegments", nDTSegments, "",  nanoaod::FlatTable::FloatColumn);
  dsaMuonTab->addColumn<float>("nCSCSegments", nCSCSegments, "",  nanoaod::FlatTable::FloatColumn);

  iEvent.put(std::move(dsaMuonTab), "DSAMuon");
}

bool DSAMuonTableProducer::passesDisplacedID(const reco::Track& dsaMuon) const {
  // displaced muon Id as recommended by Muon POG
  float validHits =  dsaMuon.hitPattern().numberOfValidMuonCSCHits() + dsaMuon.hitPattern().numberOfValidMuonDTHits();
  if(validHits > 12){
    if(dsaMuon.hitPattern().numberOfValidMuonCSCHits() != 0 || (dsaMuon.hitPattern().numberOfValidMuonCSCHits() == 0 && dsaMuon.hitPattern().numberOfValidMuonDTHits() > 18)){
      if(dsaMuon.normalizedChi2() < 2.5) {
        if(dsaMuon.ptError()/dsaMuon.pt() < 1){
          return true;
        }
      }
    }
  }
  return false;
}

int DSAMuonTableProducer::getMatches(const pat::Muon& muon, const reco::Track& dsaMuon, float minPositionDiff) const {

  int nMatches = 0;
  
  for (auto& hit : dsaMuon.recHits()){

    if (!hit->isValid()) continue;
    DetId id = hit->geographicalId();
    if (id.det() != DetId::Muon) continue;

    if (id.subdetId() == MuonSubdetId::DT || id.subdetId() == MuonSubdetId::CSC){

      for (auto& chamber : muon.matches()) {

        if (chamber.id.rawId() != id.rawId()) continue;

        for (auto& segment : chamber.segmentMatches) {
          
          if (fabs(segment.x - hit->localPosition().x()) < minPositionDiff &&
              fabs(segment.y - hit->localPosition().y()) < minPositionDiff) {
              nMatches++;
              break;
          }
        }
      }
    }
  }
  return nMatches;
}

int DSAMuonTableProducer::getDTMatches(const pat::Muon& muon, const reco::Track& dsaMuon, float minPositionDiff) const {

  int nMatches = 0;

  for (auto& hit : dsaMuon.recHits()){

    if (!hit->isValid()) continue;
    DetId id = hit->geographicalId();
    if (id.det() != DetId::Muon) continue;

    if (id.subdetId() == MuonSubdetId::DT){

      for (auto& chamber : muon.matches()) {

        if (chamber.id.rawId() != id.rawId()) continue;

        for (auto& segment : chamber.segmentMatches) {

          if (fabs(segment.x - hit->localPosition().x()) < minPositionDiff &&
              fabs(segment.y - hit->localPosition().y()) < minPositionDiff) {
              nMatches++;
              break;
          }
        }
      }
    }
  }
  return nMatches;
}

int DSAMuonTableProducer::getCSCMatches(const pat::Muon& muon, const reco::Track& dsaMuon, float minPositionDiff) const {

  int nMatches = 0;

  for (auto& hit : dsaMuon.recHits()){

    if (!hit->isValid()) continue;
    DetId id = hit->geographicalId();
    if (id.det() != DetId::Muon) continue;

    if (id.subdetId() == MuonSubdetId::CSC){

      for (auto& chamber : muon.matches()) {

        if (chamber.id.rawId() != id.rawId()) continue;

        for (auto& segment : chamber.segmentMatches) {

          if (fabs(segment.x - hit->localPosition().x()) < minPositionDiff &&
              fabs(segment.y - hit->localPosition().y()) < minPositionDiff) {
              nMatches++;
              break;
          }
        }
      }
    }
  }
  return nMatches;
}

int DSAMuonTableProducer::getTotSegments(const reco::Track& dsaMuon) const {
  int nHits = 0;
  for (auto& hit : dsaMuon.recHits()){
    if (!hit->isValid()) continue;
    DetId id = hit->geographicalId();
    if (id.det() != DetId::Muon) continue;
    if (id.subdetId() == MuonSubdetId::DT || id.subdetId() == MuonSubdetId::CSC) nHits++;
  }
  return nHits;
}

int DSAMuonTableProducer::getDTSegments(const reco::Track& dsaMuon) const {
  int nHits = 0;
  for (auto& hit : dsaMuon.recHits()){
    if (!hit->isValid()) continue;
    DetId id = hit->geographicalId();
    if (id.det() != DetId::Muon) continue;
    if (id.subdetId() == MuonSubdetId::DT) nHits++;
  }
  return nHits;
}

int DSAMuonTableProducer::getCSCSegments(const reco::Track& dsaMuon) const {
  int nHits = 0;
  for (auto& hit : dsaMuon.recHits()){
    if (!hit->isValid()) continue;
    DetId id = hit->geographicalId();
    if (id.det() != DetId::Muon) continue;
    if (id.subdetId() == MuonSubdetId::CSC) nHits++;
  }
  return nHits;
}

#include "FWCore/Framework/interface/MakerMacros.h"
DEFINE_FWK_MODULE(DSAMuonTableProducer);
