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
#include "BotManagerFuncs.h"

#include "BotGameStateFuncs.h"

#include "BotBuilderFuncs.h"

#include "BotAttackManager.h"


  sc2::Tag scout_marine_id = 0; // 0 means no scout assigned yet
  size_t current_target_index = 0; // Index of the current enemy start position to scout
  sc2::Point2D enemy_base_location; // Enemy base location when identified
  bool scout_died = false;          // Flag to indicate if the scout has died
  sc2::Point2D base_location = sc2::Point2D();
  sc2::Point2D satellite_location = sc2::Point2D();
  sc2::Point2D main_base_mineral_patch = sc2::Point2D();
  bool satellite_built = false;
};

#endif