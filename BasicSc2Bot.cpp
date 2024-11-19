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

// manage scv's will limit our scv production to 31
void BasicSc2Bot::ManageSCVs() {
  // Force SCVs to build refineries and harvest vespene
  ForceSCVsToBuildAndHarvest();
}

void BasicSc2Bot::ForceSCVsToBuildAndHarvest() {
  const ObservationInterface *observation = Observation();
  Units scvs = observation->GetUnits(Unit::Alliance::Self,
                                     IsUnit(UNIT_TYPEID::TERRAN_SCV));
  Units geysers = observation->GetUnits(
      Unit::Alliance::Neutral, IsUnit(UNIT_TYPEID::NEUTRAL_VESPENEGEYSER));

  // Get command center position
  const Unit *command_center =
      observation
          ->GetUnits(Unit::Alliance::Self,
                     IsUnit(UNIT_TYPEID::TERRAN_COMMANDCENTER))
          .front();
  Point2D base_position = command_center->pos;

  int scv_count = 0;
  int refinery_count = CountUnits(UNIT_TYPEID::TERRAN_REFINERY);
  // Build refineries if fewer than 2 exist
  if (refinery_count < 2) {
    // for each loop, https://www.w3schools.com/cpp/cpp_for_loop_foreach.asp,
    // very useful
    for (const auto &geyser : geysers) {
      // Check distance from the base
      float distance = Distance2D(geyser->pos, base_position);
      if (distance < 20.0f) { // Only consider geysers within 20 units
        for (const auto &scv : scvs) {
          if (scv_count < 10 && scv->orders.empty()) {
            Actions()->UnitCommand(scv, ABILITY_ID::BUILD_REFINERY, geyser);
            ++scv_count;
            break;
          }
        }
      }
    }
  }
}

void BasicSc2Bot::ManageTroopsAndBuildings() {

  // ----------------- Barracks -----------------
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

  // ----------------- Starport and Factory -----------------
  // Check if we need to build a Starport
  // only buold a startport if we have a factory
  if (CountUnits(UNIT_TYPEID::TERRAN_STARPORT) < 1 &&
      CountUnits(UNIT_TYPEID::TERRAN_STARPORT) < 1) {
    TryBuildStructure(ABILITY_ID::BUILD_STARPORT, UNIT_TYPEID::TERRAN_SCV);
  }

  // Ensure we have a Tech Lab on the Starport before training Banshees
  Units starports = Observation()->GetUnits(
      Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_STARPORT));
  for (const auto &starport : starports) {
    if (starport->add_on_tag == NullTag) { // Check if Starport has no add-on
                                           // i.e. there is no tehc lab
      Actions()->UnitCommand(starport, ABILITY_ID::BUILD_TECHLAB);
    }
  }

  // Train Banshees if we have a Tech Lab on the Starport
  // cap the banshhes to 10 and only make them if we have at least 5 tanks
  if (CountUnits(UNIT_TYPEID::TERRAN_BANSHEE) < 10 &&
      CountUnits(UNIT_TYPEID::TERRAN_SIEGETANK) >= 5) {
    for (const auto &starport : starports) {
      if (starport->add_on_tag != NullTag) { // Ensure Starport has a Tech Lab
        const Unit *add_on = Observation()->GetUnit(starport->add_on_tag);
        if (add_on &&
            add_on->unit_type == UNIT_TYPEID::TERRAN_STARPORTTECHLAB &&
            starport->orders.empty()) {
          Actions()->UnitCommand(starport, ABILITY_ID::TRAIN_BANSHEE);
        }
      }
    }
  }

  // Check if enough Marines are trained, then build 1 factory
  if (CountUnits(UNIT_TYPEID::TERRAN_MARINE) >= 30) {
    if (CountUnits(UNIT_TYPEID::TERRAN_FACTORY) < 1) {
      TryBuildStructure(ABILITY_ID::BUILD_FACTORY, UNIT_TYPEID::TERRAN_SCV);
    }
  }

  // Attach a Tech Lab to each Factory
  // tech lab needed on factory to build siege tanks
  Units factories = Observation()->GetUnits(
      Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_FACTORY));
  for (const auto &factory : factories) {
    if (factory->add_on_tag == NullTag) { // Check if teh factory has no add-on,
                                          // i.e. there is no tehc lab
      Actions()->UnitCommand(factory, ABILITY_ID::BUILD_TECHLAB);
    }
  }

  // cap siege tanks at 20 units
  if (CountUnits(UNIT_TYPEID::TERRAN_SIEGETANK) < 20) {
    // Train Siege Tanks only in factories with Tech Labs
    for (const auto &factory : factories) {
      if (factory->add_on_tag != NullTag) { // Ensure factory has a Tech Lab
        const Unit *add_on = Observation()->GetUnit(factory->add_on_tag);
        if (add_on && add_on->unit_type == UNIT_TYPEID::TERRAN_FACTORYTECHLAB &&
            factory->orders.empty()) {
          Actions()->UnitCommand(factory, ABILITY_ID::TRAIN_SIEGETANK);
        }
      }
    }
  }
}

void BasicSc2Bot::TrainStarportUnits() {
  // Train Banshees
  int bansheeCount = CountUnits(UNIT_TYPEID::TERRAN_BANSHEE);
  Units starports = Observation()->GetUnits(
      Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_STARPORT));

  for (const auto &starport : starports) {
    // Check if the Starport has a Tech Lab attached
    if (starport->add_on_tag != NullTag) {
      const Unit *add_on = Observation()->GetUnit(starport->add_on_tag);
      if (add_on && add_on->unit_type == UNIT_TYPEID::TERRAN_STARPORTTECHLAB) {
        // Limit Banshees to 10
        if (starport->orders.empty() && bansheeCount < 10) {
          Actions()->UnitCommand(starport, ABILITY_ID::TRAIN_BANSHEE);
        }
      }
    }
  }

  // TODO: implement building medivacs later, seem to be useful cause they can
  // heal "biological troops"
  // https://liquipedia.net/starcraft2/Medivac_(Legacy_of_the_Void)
}

void BasicSc2Bot::ManageSupply() { TryBuildSupplyDepot(); }

// Attempts to build specified structures - from tutorial
bool BasicSc2Bot::TryBuildStructure(ABILITY_ID ability_type_for_structure,
                                    UNIT_TYPEID unit_type) {
  const ObservationInterface *observation = Observation();
  const Unit *unit_to_build = nullptr;
  Units units = observation->GetUnits(Unit::Alliance::Self);

  // for each loop, https://www.w3schools.com/cpp/cpp_for_loop_foreach.asp,
  // very useful
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

// from tutorial
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

// helper funciton to determine how many of a cetain unit we have
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