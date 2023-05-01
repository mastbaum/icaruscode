// icaruscode includes
#include "sbnobj/Common/CRT/CRTHit.hh"
#include "icaruscode/CRT/CRTUtils/CRTHitRecoAlg.h"
#include "sbnobj/Common/Trigger/ExtraTriggerInfo.h"
#include "icaruscode/IcarusObj/CRTPMTMatching.h"
#include "icaruscode/CRT/CRTUtils/CRTPMTMatchingUtils.h"

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
#include "canvas/Utilities/Exception.h"

// C++ includes
#include <memory>
#include <numeric>
#include <iostream>
#include <map>
#include <iterator>
#include <algorithm>
#include <vector>

// LArSoft

#include "icaruscode/Decode/DataProducts/TriggerConfiguration.h"
#include "larcore/Geometry/Geometry.h"
#include "larcore/Geometry/AuxDetGeometry.h"
#include "larcorealg/Geometry/GeometryCore.h"
#include "larcore/CoreUtils/ServiceUtil.h" // lar::providerFrom()
#include "lardata/Utilities/AssociationUtil.h"
#include "larcorealg/CoreUtils/enumerate.h"
#include "lardata/DetectorInfoServices/LArPropertiesService.h"
#include "lardata/DetectorInfoServices/DetectorPropertiesService.h"
#include "lardata/DetectorInfoServices/DetectorClocksService.h"
#include "lardataobj/RawData/ExternalTrigger.h"
#include "larcoreobj/SimpleTypesAndConstants/PhysicalConstants.h"
#include "larcoreobj/SimpleTypesAndConstants/geo_types.h"
#include "lardataobj/RecoBase/OpHit.h"
#include "lardataobj/RecoBase/OpFlash.h"
#include "lardata/Utilities/AssociationUtil.h"

// ROOT
#include "TTree.h"
#include "TFile.h"
#include "TH1D.h"
#include "TH2D.h"
#include "TVector3.h"
#include "TGeoManager.h"

using std::vector;

namespace icarus::crt {

  class CRTPMTMatchingProducer : public art::EDProducer {
  public:
 
    using CRTHit = sbn::crt::CRTHit;
    
    explicit CRTPMTMatchingProducer(fhicl::ParameterSet const & p);

    // The destructor generated by the compiler is fine for classes
    // without bare pointers or other resource use.

    // Plugins should not be copied or assigned.
    CRTPMTMatchingProducer(CRTPMTMatchingProducer const &) = delete;
    CRTPMTMatchingProducer(CRTPMTMatchingProducer &&) = delete;
    CRTPMTMatchingProducer & operator = (CRTPMTMatchingProducer const &) = delete; 
    CRTPMTMatchingProducer & operator = (CRTPMTMatchingProducer &&) = delete;

    // Required functions.
    void produce(art::Event & e) override;

    // Selected optional functions.
    void beginJob() override;
    void beginRun(art::Run& run) override;
    void endJob() override;

    //void reconfigure(fhicl::ParameterSet const & p);

  private:

	// Params from fcl file.......

	std::vector<art::InputTag> fFlashLabels;

  	// art::InputTag fOpHitModuleLabel;
  	art::InputTag fOpFlashModuleLabel0;		///< OpticalFlashes Cryo 0.
  	art::InputTag fOpFlashModuleLabel1;		///< OpticalFlashes Cryo1.
	art::InputTag fCrtHitModuleLabel;		///< name of crt producer.
	art::InputTag fTriggerLabel;		        ///< name of trigger producer.
	art::InputTag fTriggerConfigurationLabel;	///< name of the trigger configuration.

        icarus::TriggerConfiguration fTriggerConfiguration;
    
	double fTimeOfFlightInterval;	 		///< CRTPMT time difference interval to find the match.
	int fPMTADCThresh;				///< ADC amplitude for a PMT to be considered above threshold.
	int fnOpHitToTrigger;				///< Number of OpHit above threshold to mimic the triggering PMT.
	double fBNBBeamGateMin;
	double fBNBBeamGateMax;
	double fBNBinBeamMin;
	double fBNBinBeamMax;
	double fNuMIBeamGateMin;
	double fNuMIBeamGateMax;
	double fNuMIinBeamMin;
	double fNuMIinBeamMax;

	// Trigger data product variables
	unsigned int m_gate_type;
	std::string m_gate_name;
	uint64_t m_trigger_timestamp;
	uint64_t m_gate_start_timestamp;
	uint64_t m_trigger_gate_diff;
	uint64_t m_gate_width;


	geo::GeometryCore const* fGeometryService;	///< pointer to Geometry provider

  }; // class CRTPMTMatchingProducer

  CRTPMTMatchingProducer::CRTPMTMatchingProducer(fhicl::ParameterSet const & p)
  : EDProducer{p},
      fOpFlashModuleLabel0(
          p.get<art::InputTag>("OpFlashModuleLabel0")),
      fOpFlashModuleLabel1(
          p.get<art::InputTag>("OpFlashModuleLabel1")),
      fCrtHitModuleLabel(p.get<art::InputTag>("CrtHitModuleLabel", "crthit")),
      fTriggerLabel(p.get<art::InputTag>("TriggerLabel", "daqTrigger")),
      fTriggerConfigurationLabel(
          p.get<art::InputTag>("TriggerConfiguration", "triggerconfig")),
      fTimeOfFlightInterval(p.get<double>("TimeOfFlightInterval")),
      fPMTADCThresh(p.get<int>("PMTADCThresh")),
      fnOpHitToTrigger(p.get<int>("nOpHitToTrigger")),
      fBNBBeamGateMin(p.get<double>("BNBBeamGateMin")),
      fBNBBeamGateMax(p.get<double>("BNBBeamGateMax")),
      fBNBinBeamMin(p.get<double>("BNBinBeamMin")),
      fBNBinBeamMax(p.get<double>("BNBinBeamMax")),
      fNuMIBeamGateMin(p.get<double>("NuMIBeamGateMin")),
      fNuMIBeamGateMax(p.get<double>("NuMIBeamGateMax")),
      fNuMIinBeamMin(p.get<double>("NuMIinBeamMin")),
      fNuMIinBeamMax(p.get<double>("NuMIinBeamMax")),
      fGeometryService(lar::providerFrom<geo::Geometry>())
        {
	  produces< std::vector<CRTPMTMatching> >();
	  fFlashLabels = { fOpFlashModuleLabel0, fOpFlashModuleLabel1 };
	} // CRTPMTMatchingProducer()

  /*void CRTPMTMatchingProducer::reconfigure(fhicl::ParameterSet const & p)
  {
    fCrtModuleLabel = (p.get<art::InputTag> ("CrtModuleLabel")); 
    fTriggerLabel   = (p.get<art::InputTag> ("TriggerLabel")); 
  } // CRTPMTMatchingProducer::reconfigure() */

  void CRTPMTMatchingProducer::beginRun(art::Run& r) {
	fTriggerConfiguration =
      		r.getProduct<icarus::TriggerConfiguration>(fTriggerConfigurationLabel);
  }

  void CRTPMTMatchingProducer::beginJob()
  {

  } // CRTPMTMatchingProducer::beginJob()
 
  void CRTPMTMatchingProducer::produce(art::Event & e)
  {
	mf::LogDebug("CRTPMTMatchingProducer: ") << "beginning CRTPMTProducer";
	std::cout<<"LETS START PRODUCING"<<std::endl;
	std::unique_ptr< vector<CRTPMTMatching> > CRTPMTMatchesColl( new vector<CRTPMTMatching>);
	//std::unique_ptr< art::Assns<CRTPMTMatching, recob::OpFlash> > FlashAssociation( new art::Assns<CRTPMTMatching, recob::OpFlash>);

    	//art::PtrMaker<sbn::crt::CRTHit> makeHitPtr(event);
 	// add trigger info
  	auto const& triggerInfo = e.getProduct<sbn::ExtraTriggerInfo>(fTriggerLabel);
  	sbn::triggerSource bit = triggerInfo.sourceType;
  	m_gate_type = (unsigned int)bit;
  	m_gate_name = bitName(bit);
  	m_trigger_timestamp = triggerInfo.triggerTimestamp;
  	m_gate_start_timestamp = triggerInfo.beamGateTimestamp;
  	m_trigger_gate_diff = triggerInfo.triggerTimestamp - triggerInfo.beamGateTimestamp;
  	m_gate_width = fTriggerConfiguration.getGateWidth(m_gate_type);
	
	// add CRTHits
  	art::Handle<std::vector<CRTHit>> crtHitListHandle;
  	std::vector<art::Ptr<CRTHit>> crtHitList;
  	if (e.getByLabel(fCrtHitModuleLabel, crtHitListHandle))
    	  art::fill_ptr_vector(crtHitList, crtHitListHandle);

	// add optical flashes
 	for (art::InputTag const& flashLabel : fFlashLabels) {
    		auto const flashHandle =
        	  e.getHandle<std::vector<recob::OpFlash>>(flashLabel);
    		art::FindMany<recob::OpHit> findManyHits(flashHandle, e, flashLabel);

		std::vector<FlashType> thisEventFlashes;

		for (auto const& [iflash, flash] : util::enumerate(*flashHandle)) {
			enum matchType eventType = others;
			double tflash = flash.Time();
      			vector<recob::OpHit const*> const& hits = findManyHits.at(iflash);
      			int nPMTsTriggering = 0;
      			double firstTime = 999999;
      			geo::vect::MiddlePointAccumulator flashCentroid;
      			for (auto const& hit : hits) {
        			if (hit->Amplitude() > fPMTADCThresh) nPMTsTriggering++;
        			if (firstTime > hit->PeakTime()) firstTime = hit->PeakTime();
        			geo::Point_t const pos =
            		  	  fGeometryService->OpDetGeoFromOpChannel(hit->OpChannel())
                	  	  .GetCenter();
        			double amp = hit->Amplitude();
        			flashCentroid.add(pos, amp);
      			}
      			geo::Point_t flash_pos = flashCentroid.middlePoint();
      			if (nPMTsTriggering < fnOpHitToTrigger) continue;

			bool inTime = flashInTime(firstTime, m_gate_type, m_trigger_gate_diff,
                                m_gate_width);
			double fThisRelGateTime = m_trigger_gate_diff + tflash * 1e3;
      			bool fThisInTime_gate = false;
     			bool fThisInTime_beam = false;
      			if (m_gate_type == 1 || m_gate_type == 3) {  // BNB OffBeamBNB
        			if (fThisRelGateTime > fBNBBeamGateMin &&
            			  fThisRelGateTime < fBNBBeamGateMax)
          			    fThisInTime_gate = true;
        			if (fThisRelGateTime > fBNBinBeamMin &&
            			  fThisRelGateTime < fBNBinBeamMax)
          			    fThisInTime_beam = true;
      			}
      			if (m_gate_type == 2 || m_gate_type == 4) {  // NuMI OffBeamNuMI
        			if (fThisRelGateTime > fNuMIBeamGateMin &&
            			  fThisRelGateTime < fNuMIBeamGateMax)
          			    fThisInTime_gate = true;
        			if (fThisRelGateTime > fNuMIinBeamMin &&
           			  fThisRelGateTime < fNuMIinBeamMax)
          			    fThisInTime_beam = true;
      			}
      			inTime = fThisInTime_gate;
		        icarus::crt::CRTMatches CRTmatches = CRTHitmatched(
          		  firstTime, flash_pos, crtHitList, fTimeOfFlightInterval);
			int TopEn = 0, TopEx = 0, SideEn = 0, SideEx = 0;
      			auto nCRTHits = CRTmatches.entering.size() + CRTmatches.exiting.size();
 			std::vector<MatchedCRT> thisFlashCRTmatches;
			eventType = CRTmatches.FlashType;
			if (nCRTHits > 0) {
        			for (auto const& entering : CRTmatches.entering) {
          				vector<double> CRTpos {entering.CRTHit->x_pos,
                                 		entering.CRTHit->y_pos,
                                 		entering.CRTHit->z_pos};
          				geo::Point_t thisCRTpos {entering.CRTHit->x_pos,
                                   		entering.CRTHit->y_pos,
                                   		entering.CRTHit->z_pos};
          				double CRTtime = entering.CRTHit->ts1_ns / 1e3;
          				int CRTRegion = entering.CRTHit->plane;
          				int CRTSys = 0;
          				if (CRTRegion >= 36) CRTSys = 1;  	// Very lazy way to determine if the Hit is a Top or a Side.
                    					    			// Will update it when bottom CRT will be availble.
          				double CRTTof_opflash = entering.CRTHit->ts1_ns - tflash * 1e3;
          				std::vector<int> HitFebs;
          				for (auto crts : entering.CRTHit->feb_id) {
						HitFebs.emplace_back((int)crts);
					}
          				if (CRTSys == 0) TopEn++;
          				if (CRTSys == 1) SideEn++;		
					//matchedCRT thisCRTMatch = { /* .CRTHitPos = */ thisCRTpos, // C++20: restore initializers
					MatchedCRT thisCRTMatch = { /* .CRTHitPos = */ thisCRTpos, // C++20: restore initializers
								    /* .CRTPMTTimeDiff_ns = */ CRTTof_opflash,
								    /* .CRTTime_us = */ CRTtime,
								    /* .CRTSys = */ CRTSys,
								    /* .CRTRegion = */ CRTRegion};	
					thisFlashCRTmatches.push_back(thisCRTMatch);
        			}
				for (auto const& exiting : CRTmatches.exiting) {
          				vector<double> CRTpos {exiting.CRTHit->x_pos,
                                 		exiting.CRTHit->y_pos,
                                 		exiting.CRTHit->z_pos};
          				geo::Point_t thisCRTpos {exiting.CRTHit->x_pos,
                                   		exiting.CRTHit->y_pos,
                                   		exiting.CRTHit->z_pos};
          				double CRTtime = exiting.CRTHit->ts1_ns / 1e3;
          				int CRTRegion = exiting.CRTHit->plane;
          				int CRTSys = 0;
          				if (CRTRegion >= 36) CRTSys = 1; 
          				double CRTTof_opflash = exiting.CRTHit->ts1_ns - tflash * 1e3;
          				std::vector<int> HitFebs;
          				for (auto crts : exiting.CRTHit->feb_id) {
						HitFebs.emplace_back((int)crts);
					}
          				if (CRTSys == 0) TopEx++;
          				if (CRTSys == 1) SideEx++;		
					MatchedCRT thisCRTMatch = { /* .CRTHitPos =*/  thisCRTpos, // C++20: restore initializers
                                     	  /* .CRTPMTTimeDiff_ns = */ CRTTof_opflash,
                                     	  /* .CRTTime_us = */ CRTtime,
                                     	  /* .CRTSys = */ CRTSys,
                                     	  /* .CRTRegion = */ CRTRegion};	
					thisFlashCRTmatches.push_back(thisCRTMatch);
        			}
			} //Fine CRT
			FlashType thisFlashType = { /* .FlashPos = */ flash_pos, // C++20: restore initializers
                                 /* .FlashTime_us = */ tflash,
                                 /* .FlashGateTime_ns = */ fThisRelGateTime,
                                 /* .inBeam = */ fThisInTime_beam,
                                 /* .inGate = */ inTime,
                                 /* .Classification = */ eventType,
                                 /* .CRTmatches = */ thisFlashCRTmatches};
			thisEventFlashes.push_back(thisFlashType);
		} // Fine di questo flash
		for (auto const& theseFlashes : thisEventFlashes){
			CRTPMTMatching ProducedFlash = FillCRTPMT (theseFlashes, e.id().event(), e.run(), m_gate_type);
			CRTPMTMatchesColl->push_back(ProducedFlash);	
		}
	}
   //art::PtrMaker<sbn::crt::CRTHit> makeHitPtr(event);
	std::cout<<"This Event has "<<CRTPMTMatchesColl->size()<<" Flashes."<<std::endl;
    e.put(std::move(CRTPMTMatchesColl));
    //e.put(std::move(FlashAssociation));

  } // CRTPMTMatchingProducer::produce()

  void CRTPMTMatchingProducer::endJob()
  {

  } // CRTPMTMatchingProducer::endJob()

  DEFINE_ART_MODULE(CRTPMTMatchingProducer)

} //end namespace
