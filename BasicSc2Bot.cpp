#include "BasicSc2Bot.h"

using namespace sc2;

void BasicSc2Bot::OnGameStart() { std::cout << "Go!" << std::endl; }

void BasicSc2Bot::OnStep() { BasicSc2Bot::TryBuildSupplyDepot(); }

// will run everytime a unit is idle
void BasicSc2Bot::OnUnitIdle(const sc2::Unit *unit) {
  switch (unit->unit_type.ToType()) {

  // if the unit is a command center, train an SCV
  case UNIT_TYPEID::TERRAN_COMMANDCENTER: {
    Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_SCV);
    break;
  }
  default: {
    break;
  }
  }
}

bool BasicSc2Bot::TryBuildStructure(ABILITY_ID ability_type_for_structure,
                                    UNIT_TYPEID unit_type) {
  const ObservationInterface *observation = Observation();

  // If a unit already is building a supply structure of this type, do nothing.
  // Also get an scv to build the structure.
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

  float rx = GetRandomScalar();
  float ry = GetRandomScalar();

  Actions()->UnitCommand(unit_to_build, ability_type_for_structure,
                         Point2D(unit_to_build->pos.x + rx * 15.0f,
                                 unit_to_build->pos.y + ry * 15.0f));
  return true;
}

bool BasicSc2Bot::TryBuildSupplyDepot() {
  const ObservationInterface *observation = Observation();

  // If we are not supply capped, don't build a supply depot.
  if (observation->GetFoodUsed() <= observation->GetFoodCap() - 2)
    return false;

  // Try and build a depot. Find a random SCV and give it the order.
  return BasicSc2Bot::TryBuildStructure(ABILITY_ID::BUILD_SUPPLYDEPOT);
}
