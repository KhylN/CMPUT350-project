#include "BasicSc2Bot.h"
using namespace sc2;

void BasicSc2Bot::OnGameStart() { std::cout << "Go!" << std::endl; }

void BasicSc2Bot::OnStep() {
  ManageSupply();
  ManageSCVs();
  ManageTroopsAndBuildings();
}

// Will run every time a unit is idle
void BasicSc2Bot::OnUnitIdle(const sc2::Unit *unit) {
  switch (unit->unit_type.ToType()) {
  case UNIT_TYPEID::TERRAN_COMMANDCENTER: {
    if (CountUnits(UNIT_TYPEID::TERRAN_SCV) < 31) {
      Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_SCV);
    }
    break;
  }
  case UNIT_TYPEID::TERRAN_SCV: {
    const Unit *mineral_target = FindNearestMineralPatch(unit->pos);
    if (mineral_target) {
      Actions()->UnitCommand(unit, ABILITY_ID::SMART, mineral_target);
    }
    break;
  }
  case UNIT_TYPEID::TERRAN_BARRACKS: {
    if (CountUnits(UNIT_TYPEID::TERRAN_MARINE) < 30) {
      Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_MARINE);
    }
    break;
  }
  default:
    break;
  }
}

// manage scv's willl limit our scv production to 31
void BasicSc2Bot::ManageSCVs() {
  // Train more SCVs if there are fewer than 31
  if (CountUnits(UNIT_TYPEID::TERRAN_SCV) < 31) {
    TryBuildStructure(ABILITY_ID::TRAIN_SCV, UNIT_TYPEID::TERRAN_COMMANDCENTER);
  }

  // force SCVs to build refineries and harvest vespene
  ForceSCVsToBuildAndHarvest();
}

void BasicSc2Bot::ForceSCVsToBuildAndHarvest() {
  const ObservationInterface *observation = Observation();
  Units scvs = observation->GetUnits(Unit::Alliance::Self,
                                     IsUnit(UNIT_TYPEID::TERRAN_SCV));
  Units geysers = observation->GetUnits(
      Unit::Alliance::Neutral, IsUnit(UNIT_TYPEID::NEUTRAL_VESPENEGEYSER));

  // get command center position
  const Unit *command_center =
      observation
          ->GetUnits(Unit::Alliance::Self,
                     IsUnit(UNIT_TYPEID::TERRAN_COMMANDCENTER))
          .front();
  Point2D base_position = command_center->pos;

  int scv_count = 0;
  int refinery_count = CountUnits(UNIT_TYPEID::TERRAN_REFINERY);

  // force at least 10 SCVs are focused on vespene
  for (const auto &geyser : geysers) {
    // command SCVs to harvest vespene
    for (const auto &scv : scvs) {
      if (scv_count < 10 &&
          scv->orders.empty()) { // Ensure at least 10 SCVs are busy harvesting
        Actions()->UnitCommand(scv, ABILITY_ID::SMART, geyser);
        ++scv_count;
      }
    }
  }

  // Build refineries if fewer than 2 exist
  if (refinery_count < 2) {
    scv_count = 0; // Reset SCV count for building refineries
    // Loop through geysers again to find the closest ones
    for (const auto &geyser : geysers) {
      // Check distance from the base
      float distance = Distance2D(geyser->pos, base_position);

      if (distance < 20.0f) { // find geysers within 20 units of distance
        // Find an available SCV to build a refinery
        for (const auto &scv : scvs) {
          if (scv_count < 10 &&
              scv->orders.empty()) { // Ensure SCVs are not busy
            Actions()->UnitCommand(scv, ABILITY_ID::BUILD_REFINERY, geyser);
            ++scv_count; // Increment the count of SCVs commanded to build
            break;
          }
        }
      }
    }
  }
}

void BasicSc2Bot::ManageTroopsAndBuildings() {
  if (CountUnits(UNIT_TYPEID::TERRAN_BARRACKS) < 2) {
    TryBuildStructure(ABILITY_ID::BUILD_BARRACKS);
  }
  if (CountUnits(UNIT_TYPEID::TERRAN_MARINE) < 30) {
    Units barracks = Observation()->GetUnits(
        Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_BARRACKS));
    for (const auto &barrack : barracks) {
      if (barrack->orders.empty()) {
        Actions()->UnitCommand(barrack, ABILITY_ID::TRAIN_MARINE);
      }
    }
  }
  // Check if enough Marines are trained, then build 2 Factories
  if (CountUnits(UNIT_TYPEID::TERRAN_MARINE) >= 30) {
    if (CountUnits(UNIT_TYPEID::TERRAN_FACTORY) < 2) {
      TryBuildStructure(ABILITY_ID::BUILD_FACTORY, UNIT_TYPEID::TERRAN_SCV);
    }
  }
}

void BasicSc2Bot::ManageSupply() { TryBuildSupplyDepot(); }

// Attempts to build specified structures
bool BasicSc2Bot::TryBuildStructure(ABILITY_ID ability_type_for_structure,
                                    UNIT_TYPEID unit_type) {
  const ObservationInterface *observation = Observation();
  const Unit *unit_to_build = nullptr;
  Units units = observation->GetUnits(Unit::Alliance::Self);

  for (const auto &unit : units) {
    for (const auto &order : unit->orders) {
      if (order.ability_id == ability_type_for_structure) {
        return false;
      }
    }
    if (unit->unit_type == unit_type) {
      unit_to_build = unit;
    }
  }
  if (!unit_to_build) {
    return false;
  }

  float rx = GetRandomScalar();
  float ry = GetRandomScalar();
  Actions()->UnitCommand(unit_to_build, ability_type_for_structure,
                         Point2D(unit_to_build->pos.x + rx * 15.0f,
                                 unit_to_build->pos.y + ry * 15.0f));
  return true;
}

bool BasicSc2Bot::TryBuildSupplyDepot() {
  const ObservationInterface *observation = Observation();
  if (observation->GetFoodUsed() <= observation->GetFoodCap() - 2)
    return false;
  return TryBuildStructure(ABILITY_ID::BUILD_SUPPLYDEPOT,
                           UNIT_TYPEID::TERRAN_SCV);
}

const Unit *BasicSc2Bot::FindNearestMineralPatch(const Point2D &start) {
  Units units = Observation()->GetUnits(Unit::Alliance::Neutral);
  float distance = std::numeric_limits<float>::max();
  const Unit *target = nullptr;
  for (const auto &u : units) {
    if (u->unit_type == UNIT_TYPEID::NEUTRAL_MINERALFIELD) {
      float d = DistanceSquared2D(u->pos, start);
      if (d < distance) {
        distance = d;
        target = u;
      }
    }
  }
  return target;
}

int BasicSc2Bot::CountUnits(UNIT_TYPEID unit_type) {
  const ObservationInterface *observation = Observation();
  Units units = observation->GetUnits(Unit::Alliance::Self);
  int count = 0;
  for (const auto &unit : units) {
    if (unit->unit_type == unit_type) {
      ++count;
    }
  }
  return count;
}
