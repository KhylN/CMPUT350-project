#ifndef BASIC_SC2_BOT_H_
#define BASIC_SC2_BOT_H_

#include "sc2api/sc2_api.h"
#include "sc2api/sc2_args.h"
#include "sc2api/sc2_unit_filters.h" // needed for IsUnit
#include "sc2lib/sc2_lib.h"
#include "sc2utils/sc2_arg_parser.h"
#include "sc2utils/sc2_manage_process.h"

class BasicSc2Bot : public sc2::Agent {
public:
  virtual void OnGameStart();
  virtual void OnStep();
  virtual void OnUnitIdle(const sc2::Unit *unit);

private:
  // make macroes for marines, hellions, vikings, medivacs, seigetanks
  const int THRESH = 30;
  const int MARINE = 10;
  const int HELLIONS = 20;
  const int VIKINGS = 30;
  const int MEDIVACS = 40;
  const int SEIGETANKS = 50;

  void ManageSupply();
  void ManageSCVs();
  void ManageTroopsAndBuildings();
  int CountUnits(sc2::UNIT_TYPEID unit_type);
  int MilitaryStrength();
  bool TryMorphSupplyDepot();

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

  void ManageAllTroops();

  sc2::Point2D GetBaseLocation();

  const sc2::Unit *FindNearestMineralPatch(const sc2::Point2D &start);
  const sc2::Unit *FindNearestVespene(const sc2::Point2D &start);
  void ForceSCVsToBuildAndHarvest();

  std::vector<sc2::Point2D> potential_enemy_locations_;
  size_t current_attack_wave_ = 1;

  void InitializeEnemyLocations();
  void LaunchAttack();
  bool IsArmyIdle();
  void SendArmyTo(const sc2::Point2D &target);

  sc2::Tag scout_marine_id = 0; // 0 means no scout assigned yet
  size_t current_target_index = 0; // Index of the current enemy start position to scout
  sc2::Point2D enemy_base_location; // Enemy base location when identified
  bool scout_died = false;          // Flag to indicate if the scout has died
  sc2::Point2D FindBuildLocation(sc2::Point2D base_location, sc2::ABILITY_ID ability_type);

};

#endif
