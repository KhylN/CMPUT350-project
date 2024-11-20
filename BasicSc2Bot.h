#ifndef BASIC_SC2_BOT_H_
#define BASIC_SC2_BOT_H_

#include "sc2api/sc2_api.h"
#include "sc2api/sc2_args.h"
#include "sc2lib/sc2_lib.h"
#include "sc2utils/sc2_manage_process.h"
#include "sc2utils/sc2_arg_parser.h"

class BasicSc2Bot : public sc2::Agent {
public:
	virtual void OnGameStart();
	virtual void OnStep();

private:
<<<<<<< Updated upstream
=======
  void ManageSupply();
  void ManageSCVs();
  void ManageTroopsAndBuildings();
  int CountUnits(sc2::UNIT_TYPEID unit_type);

  bool
  TryBuildStructure(sc2::ABILITY_ID ability_type_for_structure,
                    sc2::UNIT_TYPEID unit_type = sc2::UNIT_TYPEID::TERRAN_SCV);

  bool TryBuildSupplyDepot();
  bool TryBuildBarracks();
  bool TryBuildStarport();
  bool TryBuildFactory();

  void ManageBarracks();
  void ManageStarport();
  void ManageFactory();

  const sc2::Unit *FindNearestMineralPatch(const sc2::Point2D &start);
  void TrainStarportUnits();
  void ForceSCVsToBuildAndHarvest();
>>>>>>> Stashed changes
};

#endif