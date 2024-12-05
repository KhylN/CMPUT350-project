#include "BasicSc2Bot.h"
using namespace sc2;

void BasicSc2Bot::InitializeEnemyLocations() {
  const sc2::GameInfo &game_info = Observation()->GetGameInfo();
  for (const sc2::Point2D &starting_location :
       game_info.enemy_start_locations) {
    potential_enemy_locations_.push_back(starting_location);
  }
}

void BasicSc2Bot::OnStep() {
  if (step < 15) {
    step++;
    if (step > 10) {
      InitializeEnemyLocations();
      GetBaseLocation();
      InitializeSatelliteLocation();
    }
  } else {
    ManageTroopsAndBuildings();
  }
}

// Will run every time a unit is idle
void BasicSc2Bot::OnUnitIdle(const sc2::Unit *unit) {
    switch (unit->unit_type.ToType()) {
        case UNIT_TYPEID::TERRAN_COMMANDCENTER: {
            if (unit->assigned_harvesters < 18 && (CountUnits(UNIT_TYPEID::TERRAN_ORBITALCOMMAND) < 1 && CountUnits(UNIT_TYPEID::TERRAN_COMMANDCENTER) < 2)) {
                Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_SCV);
            }
            break;
        }
        case UNIT_TYPEID::TERRAN_ORBITALCOMMAND: {
            if (CountUnits(UNIT_TYPEID::TERRAN_MULE) < 4) {
                Actions()->UnitCommand(unit, ABILITY_ID::EFFECT_CALLDOWNMULE,
                                       FindNearestMineralPatch(satellite_location));
            }
            if (unit->assigned_harvesters < 10) {
                Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_SCV);
            }
            break;
        }
        case UNIT_TYPEID::TERRAN_SCV: {
            const Unit *mineral_target = FindNearestMineralPatch(unit->pos);
            const Unit *gas_target = FindNearestVespene(unit->pos);

            if (mineral_target) {
                Actions()->UnitCommand(unit, ABILITY_ID::SMART, mineral_target);
            }

            if (gas_target) {
                Actions()->UnitCommand(unit, ABILITY_ID::SMART, gas_target);
            }
            break;
        }
        case UNIT_TYPEID::TERRAN_MULE: {
            const Unit *mineral_target = FindNearestMineralPatch(unit->pos);
            if (mineral_target) {
                Actions()->UnitCommand(unit, ABILITY_ID::SMART, mineral_target);
            }
            break;
        }
        default:
            break;
    }
}


// manage scv's will limit our scv production to 31
void BasicSc2Bot::ManageSCVs() {
  const ObservationInterface *observation = Observation();
  Units scvs = observation->GetUnits(Unit::Alliance::Self,
                                     IsUnit(UNIT_TYPEID::TERRAN_SCV));
  Units refineries = observation->GetUnits(
      Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_REFINERY));
  // Assign SCVs to refineries

  // Assign Satellite Base Builder and sent to crystals as early as possible

  if (satellite_location != Point2D() &&
      ((CountUnits(UNIT_TYPEID::TERRAN_COMMANDCENTER) == 1))) {
    // Once satellite base location is defined and as long as we do not yet have
    // a 2nd base established, we assign one SCV to the crystals.
    if (scv_at_build_site_id == -1) {
      // If no SCV yet assigned, do so and mark down its ID
      for (const auto &scv : scvs) {
        if ((scv->orders.empty() ||
             scv->orders.front().ability_id == ABILITY_ID::HARVEST_GATHER) &&
            satellite_location != Point2D()) {
          // Assign 1st free SCV to be the satellite builder if available.
          scv_at_build_site_id = scv->tag;
          // std::cout << "assigned scv build id: " << scv_at_build_site_id << std::endl;
          Actions()->UnitCommand(scv, ABILITY_ID::MOVE_MOVE, satellite_location);
          break;
        }
      }
    } else {
      // If SCV is assigned, ensure it remains at the extra crystals until its
      // ordered to build the base
      for (const auto &scv : scvs) {
        // Look for the assigned SCV
        if (scv->tag == scv_at_build_site_id) {
          if (scv->orders.empty()) {
            Actions()->UnitCommand(scv, ABILITY_ID::SMART, satellite_location);
          }
          bool is_building_cc = false;
          for (const auto &order : scv->orders) {
            if (order.ability_id == ABILITY_ID::BUILD_COMMANDCENTER) {
              is_building_cc = true;
              break;
            }
          }
          if (!is_building_cc) {
            Actions()->UnitCommand(scv, ABILITY_ID::SMART, satellite_location);
          }
        }
      }
    }
  }

  for (const auto &refinery : refineries) {
    int scv_count = 0;

    // Count the number of SCVs already harvesting from this refinery
    for (const auto &scv : scvs) {
      if (!scv->orders.empty() &&
          scv->orders.front().target_unit_tag == refinery->tag) {
        ++scv_count;
      }
    }

    // Ensure exactly 3 SCVs per refinery
    if (scv_count < 3) {
      for (const auto &scv : scvs) {
        if (scv_count >= 3) {
          break; // Stop assigning SCVs once the max is reached
        }

        bool unit_is_busy = false;
        // Only select idle SCVs or those not assigned to other tasks
        for (const auto &order : scv->orders) {
          if (order.ability_id == ABILITY_ID::BUILD_SUPPLYDEPOT ||
              order.ability_id == ABILITY_ID::BUILD_BARRACKS ||
              order.ability_id == ABILITY_ID::BUILD_FACTORY ||
              order.ability_id == ABILITY_ID::BUILD_STARPORT ||
              order.ability_id == ABILITY_ID::BUILD_REFINERY ||
              order.ability_id == ABILITY_ID::BUILD_COMMANDCENTER ||
              order.ability_id == ABILITY_ID::BUILD_ENGINEERINGBAY) {
            unit_is_busy = true;
            break;
          } else {
            Actions()->UnitCommand(scv, ABILITY_ID::SMART, refinery);
            ++scv_count;
            break;
          }
        }
        if (unit_is_busy) {
          // Do not order the busy unit.
          continue;
        }
      }
    } else if (scv_count > 3) {
      // Unassign extra SCVs if more than 3 are assigned
      for (const auto &scv : scvs) {
        if (!scv->orders.empty() &&
            scv->orders.front().target_unit_tag == refinery->tag) {
          Actions()->UnitCommand(scv, ABILITY_ID::STOP);
          --scv_count;
          if (scv_count <= 3) {
            break;
          }
        }
      }
    }
  }
}

void BasicSc2Bot::ManageTroopsAndBuildings() {
  //ManageCC();

  TryBuildRefinery();
  TryBuildSupplyDepot();
  TryMorphSupplyDepot();

  ManageSCVs();

  TryBuildNewCC();
  ManageSecondBase();

  TryBuildBarracks();

  TryBuildFactory();

  TryBuildStarport();

  ManageFactory();

  ManageBarracks();
  
  ManageStarport();

  TryBuildEnggBay();
  ManageEnggBay();

  bool can_attack = MilitaryStrength();
   std::cout << "Marines: " << CountUnits(UNIT_TYPEID::TERRAN_MARINE)
  //           << " Hellions: " << CountUnits(UNIT_TYPEID::TERRAN_HELLION)
  //           << " Vikings: " << CountUnits(UNIT_TYPEID::TERRAN_VIKINGFIGHTER)
             << " Medivacs: " << CountUnits(UNIT_TYPEID::TERRAN_MEDIVAC)
             << " Tanks: " << CountUnits(UNIT_TYPEID::TERRAN_SIEGETANK)
             << std::endl;

  if (can_attack) {
    // std::cout << "Launching attack! Wave: " << current_attack_wave_
    //           << std::endl;
    LaunchAttack();
  } else {
    ManageAllTroops();
  }
}

sc2::Point2D BasicSc2Bot::CalculateRallyPoint() {
    return sc2::Point2D((satellite_location.x),
                        (satellite_location.y));
}

void BasicSc2Bot::LaunchAttack() {
    is_attacking = true;

    if (enemy_base_location.x != 0 && enemy_base_location.y != 0) {
        sc2::Point2D rally_point = CalculateRallyPoint();

        // First ensure all troops are at the rally point
        SendArmyTo(rally_point);

        // Then launch the attack from the rally point to the enemy base
        SendArmyTo(enemy_base_location);
    }
}


void BasicSc2Bot::SendArmyTo(const sc2::Point2D &target) {
  sc2::Units marines = Observation()->GetUnits(
      sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MARINE));
  sc2::Units hellions = Observation()->GetUnits(
      sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_HELLION));
  sc2::Units tanks =
      Observation()->GetUnits(sc2::Unit::Alliance::Self,
                              sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_SIEGETANK));
  sc2::Units medivacs = Observation()->GetUnits(
      sc2::Unit::Alliance::Self, sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MEDIVAC));
  sc2::Units vikings = Observation()->GetUnits(
      sc2::Unit::Alliance::Self,
      sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_VIKINGFIGHTER));
  sc2::Units marauders =
      Observation()->GetUnits(sc2::Unit::Alliance::Self,
                              sc2::IsUnit(sc2::UNIT_TYPEID::TERRAN_MARAUDER));

  // Calculate formation points
  sc2::Point2D rally_point = sc2::Point2D((target.x + GetBaseLocation().x) / 2,
                                          (target.y + GetBaseLocation().y) / 2);

  // Create groups for medivac support assignment
  std::vector<const sc2::Unit *> ground_units;
  for (const auto &marine : marines)
    ground_units.push_back(marine);
  for (const auto &marauder : marauders)
    ground_units.push_back(marauder);

  // Send tanks first to establish position
  for (const auto &tank : tanks) {
    if (tank->orders.empty()) {
      Actions()->UnitCommand(tank, sc2::ABILITY_ID::ATTACK_ATTACK, target);
    }
  }

  // Send ground units
  for (const auto &unit : ground_units) {
    if (unit->orders.empty()) {
      Actions()->UnitCommand(unit, sc2::ABILITY_ID::ATTACK_ATTACK, target);
    }
  }
  for (const auto &hellion : hellions) {
    if (hellion->orders.empty()) {
      Actions()->UnitCommand(hellion, sc2::ABILITY_ID::ATTACK_ATTACK, target);
    }
  }

  // Assign medivacs to follow ground units
  if (!ground_units.empty() && !medivacs.empty()) {
    size_t units_per_medivac =
        std::max(ground_units.size() / medivacs.size(), size_t(1));
    size_t current_unit = 0;

    for (const auto &medivac : medivacs) {
      if (current_unit < ground_units.size()) {
        // Follow the assigned ground unit
        Actions()->UnitCommand(medivac, sc2::ABILITY_ID::SMART,
                               ground_units[current_unit]);
        current_unit += units_per_medivac;
      }
    }
  }
  // If no ground units are alive, send medivacs back to base
  else if (ground_units.empty() && !medivacs.empty()) {
    for (const auto &medivac : medivacs) {
      if (medivac->orders.empty()) {
        Actions()->UnitCommand(medivac, sc2::ABILITY_ID::MOVE_MOVE,
                               GetBaseLocation());
      }
    }
  }

  // Send vikings last
  for (const auto &viking : vikings) {
    if (viking->orders.empty()) {
      Actions()->UnitCommand(viking, sc2::ABILITY_ID::ATTACK_ATTACK, target);
    }
  }

  // Clean up: Check for idle units and direct them to attack nearby enemies
  sc2::Units enemies = Observation()->GetUnits(Unit::Alliance::Enemy);
  for (const auto &unit : ground_units) {
    if (unit->orders.empty() && !enemies.empty()) {
      Actions()->UnitCommand(unit, sc2::ABILITY_ID::ATTACK_ATTACK,
                             enemies.front()->pos);
    }
  }
  for (const auto &tank : tanks) {
    if (tank->orders.empty() && !enemies.empty()) {
      Actions()->UnitCommand(tank, sc2::ABILITY_ID::ATTACK_ATTACK,
                             enemies.front()->pos);
    }
  }
  for (const auto &hellion : hellions) {
    if (hellion->orders.empty() && !enemies.empty()) {
      Actions()->UnitCommand(hellion, sc2::ABILITY_ID::ATTACK_ATTACK,
                             enemies.front()->pos);
    }
  }
  for (const auto &viking : vikings) {
    if (viking->orders.empty() && !enemies.empty()) {
      Actions()->UnitCommand(viking, sc2::ABILITY_ID::ATTACK_ATTACK,
                             enemies.front()->pos);
    }
  }
}

bool BasicSc2Bot::HasSupportableGroundUnits() {
  return CountUnits(UNIT_TYPEID::TERRAN_MARINE) > 0 ||
         CountUnits(UNIT_TYPEID::TERRAN_MARAUDER) > 0;
}

// Attempts to build specified structures - from tutorial
bool BasicSc2Bot::TryBuildStructure(ABILITY_ID ability_type_for_structure) {
  const ObservationInterface *observation = Observation();

  // Find an available builder unit of the specified type
  const Unit *unit_to_build = nullptr;
  Units units = observation->GetUnits(Unit::Alliance::Self,
                                      IsUnit(UNIT_TYPEID::TERRAN_SCV));
  for (const auto &unit : units) {
    bool unit_is_busy = false;

    if (unit->tag == scv_at_build_site_id &&
        ability_type_for_structure != ABILITY_ID::BUILD_COMMANDCENTER) {
      continue;
    }

    if (unit->tag == scv_at_build_site_id &&
        ability_type_for_structure == ABILITY_ID::BUILD_COMMANDCENTER) {

      unit_to_build = unit;
      // std::cout << unit_to_build->tag << std::endl;
      break;
    }

    // Check if unit is already assigned to a building task
    for (const auto &order : unit->orders) {
      if (order.ability_id == ability_type_for_structure) {
        // If the unit is already building the requested structure, return early
        return false;
      }
      if (order.ability_id == ABILITY_ID::BUILD_SUPPLYDEPOT ||
          order.ability_id == ABILITY_ID::BUILD_BARRACKS ||
          order.ability_id == ABILITY_ID::BUILD_FACTORY ||
          order.ability_id == ABILITY_ID::BUILD_STARPORT ||
          order.ability_id == ABILITY_ID::BUILD_REFINERY ||
          order.ability_id == ABILITY_ID::BUILD_COMMANDCENTER ||
          order.ability_id == ABILITY_ID::BUILD_ENGINEERINGBAY) {
        unit_is_busy = true;
        break;
      }
      if (order.ability_id == ABILITY_ID::HARVEST_GATHER ||
          order.ability_id == ABILITY_ID::HARVEST_RETURN) {
        break;
      }
    }

    // Select an idle unit that matches the required type
    if (!unit_is_busy) {
      // Ennsure emplaced SCV is the only one to build new CC
      if (unit->tag != scv_at_build_site_id &&
          ability_type_for_structure == ABILITY_ID::BUILD_COMMANDCENTER) {
        continue;
      }
      unit_to_build = unit;
      break;
    }
  }

  if (!unit_to_build) {
    std::cerr << "No available builder unit found for structure: "
              << static_cast<int>(ability_type_for_structure) << std::endl;
    return false;
  }

  // Find a valid build location near the base
  Point2D base_location = GetBaseLocation();
  Point2D build_location = Point2D();
  if (ability_type_for_structure == ABILITY_ID::BUILD_MISSILETURRET) {
    build_location =
        FindBuildLocation(base_location, ability_type_for_structure, 4);
  } else if (ability_type_for_structure == ABILITY_ID::BUILD_COMMANDCENTER) {
    if (satellite_location != Point2D()) {
      build_location = satellite_location;
    } else {
      return false;
    }
  } else {
    build_location =
        FindBuildLocation(base_location, ability_type_for_structure);
  }

  if (build_location ==
      Point2D()) { // Check if FindBuildLocation returned a valid location
    // std::cerr << "No valid build location found for structure: "
    //           << static_cast<int>(ability_type_for_structure) << std::endl;
    return false;
  } else {
    // std::cout << "Build location found at: (" << build_location.x << ", "
    //           << build_location.y << ")" << std::endl;
  }

  // Check resources before issuing the build command
  auto required_resources = observation->GetUnitTypeData().at(
      static_cast<uint32_t>(ability_type_for_structure));
  if (observation->GetMinerals() < required_resources.mineral_cost ||
      observation->GetVespene() < required_resources.vespene_cost) {
    std::cerr << "Not enough resources to build structure: "
              << static_cast<int>(ability_type_for_structure) << std::endl;
    return false;
  }

  // Issue the build comand
  Actions()->UnitCommand(unit_to_build, ability_type_for_structure,
                         build_location);

  //
  // std::cout << "Issued build command to unit: " << unit_to_build->tag
  //           << " for structure: "
  //           << static_cast<int>(ability_type_for_structure) << " at location:
  //           ("
  //           << build_location.x << ", " << build_location.y << ")" <<
  //           std::endl;

  return true;
}

Point2D BasicSc2Bot::FindBuildLocation(Point2D base_location,
                                       ABILITY_ID ability_type,
                                       float build_radius) {
  const ObservationInterface *observation =
      Observation(); // Ensure we use the observation interface

  // Determine if the base is in the top-left quadrant
  bool is_top_left = base_location.x < observation->GetGameInfo().width / 2 &&
                     base_location.y > observation->GetGameInfo().height / 2;

  // Adjust the search direction based on the base location
  float dx_start = is_top_left ? build_radius : -build_radius;
  float dy_start = is_top_left ? build_radius : -build_radius;
  float dx_end = is_top_left ? -build_radius : build_radius;
  float dy_end = is_top_left ? -build_radius : build_radius;

  // Iterate over possible build locations
  for (float dx = dx_start; (is_top_left ? dx >= dx_end : dx <= dx_end);
       dx += (is_top_left ? -4.0f : 4.0f)) {
    for (float dy = dy_start; (is_top_left ? dy >= dy_end : dy <= dy_end);
         dy += (is_top_left ? -4.0f : 4.0f)) {
      Point2D current_location =
          Point2D(base_location.x + dx, base_location.y + dy);

      // Check if the build location is valid
      if (Query()->Placement(ability_type, current_location)) {
        // Ensure sufficient space for add-ons if necessary
        if (ability_type == ABILITY_ID::BUILD_FACTORY ||
            ability_type == ABILITY_ID::BUILD_STARPORT ||
            ability_type == ABILITY_ID::BUILD_BARRACKS) {
          if (Query()->Placement(
                  ability_type,
                  Point2D(current_location.x + 3.0f, current_location.y))) {
            return current_location;
          }
        } else {
          return current_location;
        }
      }
    }
  }
  return Point2D(); // Return a default empty point if no location is found
}

// MODULAR BUILDING HELPER FUNCTIONS

void BasicSc2Bot::TryBuildRefinery() {
  // Build Vespene refinery near base if there are fewer than 2
  const ObservationInterface *observation = Observation();
  Units scvs = observation->GetUnits(Unit::Alliance::Self,
                                     IsUnit(UNIT_TYPEID::TERRAN_SCV));
  Units geysers = observation->GetUnits(
      Unit::Alliance::Neutral, IsUnit(UNIT_TYPEID::NEUTRAL_VESPENEGEYSER));

  Units space_platform_geysers =
      observation->GetUnits(Unit::Alliance::Neutral,
                            IsUnit(UNIT_TYPEID::NEUTRAL_SPACEPLATFORMGEYSER));

  if (space_platform_geysers.size() > geysers.size()) {
    geysers = space_platform_geysers;
  }

  if ((CountUnits(UNIT_TYPEID::TERRAN_REFINERY) < 1 &&
       (CountUnits(UNIT_TYPEID::TERRAN_SUPPLYDEPOT) > 0 ||
        CountUnits(UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED) > 0) &&
       CountUnits(UNIT_TYPEID::TERRAN_BARRACKS) > 0) ||
      ((CountUnits(UNIT_TYPEID::TERRAN_ORBITALCOMMAND) > 0 &&
        CountUnits(UNIT_TYPEID::TERRAN_BARRACKS) > 2) &&
       CountUnits(UNIT_TYPEID::TERRAN_REFINERY) < 2)) {
    for (const auto &geyser : geysers) {
      // Check distance from the base
      float distance = Distance2D(geyser->pos, base_location);
      if (distance < 20.0f) { // Only consider geysers within 20 units
        for (const auto &scv : scvs) {
          // Find first available SCV
          
          Actions()->UnitCommand(scv, ABILITY_ID::BUILD_REFINERY, geyser);
          break;
        }
      }
    }
  } else if (CountUnits(UNIT_TYPEID::TERRAN_REFINERY) < 3 &&
             CountUnits(UNIT_TYPEID::TERRAN_MARINE) > 6 &&
             HasUpgrade(Observation(), UPGRADE_ID::STIMPACK) &&
             HasUpgrade(Observation(), UPGRADE_ID::SHIELDWALL)) {
    for (const auto &geyser : geysers) {
      // Check distance from the SATELLITE base
      float distance = Distance2D(geyser->pos, satellite_location);
      if (distance < 20.0f) { // Only consider geysers within 20 units of the
                              // satellite base
        for (const auto &scv : scvs) {
          // only select the scv's that are within 20 units of the satellite
          // base and where the scv order is not to build a refinery

          bool isBuildingRefinery = false;
          for (const auto &order : scv->orders) {
            if (order.ability_id == ABILITY_ID::BUILD_REFINERY) {
              isBuildingRefinery = true;
              break;
            }
          }

          if (Distance2D(scv->pos, satellite_location) < 20.0f &&
              !isBuildingRefinery) {
            Actions()->UnitCommand(scv, ABILITY_ID::BUILD_REFINERY, geyser);
            break;
          }
        }
      }
    }
  }
}

bool BasicSc2Bot::TryBuildSupplyDepot() {
  const ObservationInterface *observation = Observation();

  // Calculate current and planned supply capacity
  int current_supply_cap = observation->GetFoodCap();
  int used_supply = observation->GetFoodUsed();

  // Avoid building if there are already enough depots being built
  if ((CountUnits(UNIT_TYPEID::TERRAN_SUPPLYDEPOT) < 1) &&
      CountUnits(UNIT_TYPEID::TERRAN_SUPPLYDEPOTLOWERED) < 1) {
    return TryBuildStructure(ABILITY_ID::BUILD_SUPPLYDEPOT);
  }

  if (used_supply > current_supply_cap - 9 && 
      HasUpgrade(observation, UPGRADE_ID::SHIELDWALL)) {
    return TryBuildStructure(ABILITY_ID::BUILD_SUPPLYDEPOT);
  }

  if (used_supply > current_supply_cap - 4 && 
      CountUnits(UNIT_TYPEID::TERRAN_REFINERY) > 0 &&
      CountUnits(UNIT_TYPEID::TERRAN_BARRACKS) > 1) {
    return TryBuildStructure(ABILITY_ID::BUILD_SUPPLYDEPOT);
  }
  return false;
}

// Lowers all non-lowered supply depots
bool BasicSc2Bot::TryMorphSupplyDepot() {
  const ObservationInterface *observation = Observation();
  Units supply_depots = Observation()->GetUnits(
      Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_SUPPLYDEPOT));
  if (CountUnits(UNIT_TYPEID::TERRAN_SUPPLYDEPOT) > 0) {
    // If there are eligible Supply Depots, attempt to lower them.
    for (const auto *supply_depot : supply_depots) {
      if (supply_depot->orders.empty()) {
        Actions()->UnitCommand(
            supply_depot,
            ABILITY_ID::MORPH_SUPPLYDEPOT_LOWER); // Lower all idle Supply
                                                  // Depots
        return true;
      }
    }
  }
  return false;
}

bool BasicSc2Bot::TryBuildBarracks() {
  // Build barracks if we have fewer than 1.
  const ObservationInterface *observation = Observation();
  int current_supply_cap = observation->GetFoodCap();

  if (CountUnits(UNIT_TYPEID::TERRAN_BARRACKS) < 1) {
    return TryBuildStructure(ABILITY_ID::BUILD_BARRACKS);
  } else if (CountUnits(UNIT_TYPEID::TERRAN_BARRACKS) < 2 &&
             (CountUnits(UNIT_TYPEID::TERRAN_ORBITALCOMMAND) > 0 || current_supply_cap >= 25)) {
    return TryBuildStructure(ABILITY_ID::BUILD_BARRACKS);
  } else if (CountUnits(UNIT_TYPEID::TERRAN_BARRACKS) < 3 &&
             CountUnits(UNIT_TYPEID::TERRAN_MARINE) > 6 &&
             HasUpgrade(Observation(), UPGRADE_ID::SHIELDWALL)) {
    return TryBuildStructure(ABILITY_ID::BUILD_BARRACKS);
  } //else if (HasUpgrade(Observation(), UPGRADE_ID::STIMPACK) &&
    //         CountUnits(UNIT_TYPEID::TERRAN_STARPORT) > 0 &&
    //         CountUnits(UNIT_TYPEID::TERRAN_BARRACKS) < 5) {
    //return TryBuildStructure(ABILITY_ID::BUILD_BARRACKS);
  //}
  return false;
}

bool BasicSc2Bot::TryBuildNewCC() {
  // Build satellite command center to facilitate more eco. Built immediately
  // after 1st Vespene
  if (CountUnits(UNIT_TYPEID::TERRAN_REFINERY) > 0 && !satellite_built) {
    return TryBuildStructure(ABILITY_ID::BUILD_COMMANDCENTER);
  }
  return false;
}

bool BasicSc2Bot::TryBuildEnggBay() {
  const ObservationInterface *observation = Observation();
  // Build Engineering Bay if we have built or satellite base.
  // Check if all barracks have a tech lab
  if (CountUnits(UNIT_TYPEID::TERRAN_ENGINEERINGBAY) < 1 &&
      CountUnits(UNIT_TYPEID::TERRAN_MARINE) > 6 &&
      HasUpgrade(Observation(), UPGRADE_ID::SHIELDWALL)) {
    // check if an scv is already building an engg bay
    Units scvs = Observation()->GetUnits(Unit::Alliance::Self,
                                         IsUnit(UNIT_TYPEID::TERRAN_SCV));
    for (const auto &scv : scvs) {
      for (const auto &order : scv->orders) {
        if (order.ability_id == ABILITY_ID::BUILD_ENGINEERINGBAY) {
          return false;
        }
      }
    }
    return TryBuildStructure(ABILITY_ID::BUILD_ENGINEERINGBAY);
  }
  return false;
}

bool BasicSc2Bot::TryBuildMissileTurret() {
  // Build MissileTurret if we have an Engineering Bay (prereq) and if we have
  // no more than 3.
  if (CountUnits(UNIT_TYPEID::TERRAN_ENGINEERINGBAY) > 0 &&
      CountUnits(UNIT_TYPEID::TERRAN_MISSILETURRET) < 3 &&
      CountUnits(UNIT_TYPEID::TERRAN_ORBITALCOMMAND) > 0) {
    return TryBuildStructure(ABILITY_ID::BUILD_MISSILETURRET);
  }
  return false;
}

bool BasicSc2Bot::TryBuildStarport() {
  const ObservationInterface *observation = Observation();
  // Build Starport if we don't have one and if we have a Factory.
  if (CountUnits(UNIT_TYPEID::TERRAN_STARPORT) < 1 &&
      CountUnits(UNIT_TYPEID::TERRAN_MARINE) > 6 &&
      HasUpgrade(Observation(), UPGRADE_ID::SHIELDWALL)) {
    return TryBuildStructure(ABILITY_ID::BUILD_STARPORT);
  }
  return false;
}

bool BasicSc2Bot::TryBuildFactory() {
  // Build Factory if we have built the satellite base.
  if (CountUnits(UNIT_TYPEID::TERRAN_ORBITALCOMMAND) > 0) {
    if (CountUnits(UNIT_TYPEID::TERRAN_FACTORY) < 1) {
      return TryBuildStructure(ABILITY_ID::BUILD_FACTORY);
    }
  }
  return false;
}

// TROOP MANAGEMENT
void BasicSc2Bot::ManageAllTroops() {

  // place siege tanks between the satellite and the enemy base
    sc2::Point2D rally_point = CalculateRallyPoint();
    const float proximity_threshold = 10.0f;

    // Define a lambda to exclude changelings
    auto IsNotChangeling = [](const sc2::Unit &enemy) {
        return enemy.unit_type != UNIT_TYPEID::ZERG_CHANGELING &&
               enemy.unit_type != UNIT_TYPEID::ZERG_CHANGELINGZEALOT &&
               enemy.unit_type != UNIT_TYPEID::ZERG_CHANGELINGMARINESHIELD &&
               enemy.unit_type != UNIT_TYPEID::ZERG_CHANGELINGMARINE;
    };

    // Find all enemy units near our base or satellite and filter out changelings
    Units enemies_near_base = Observation()->GetUnits(
        Unit::Alliance::Enemy,
        [&](const sc2::Unit &enemy) {
            return IsNotChangeling(enemy) &&
                   (Distance2D(enemy.pos, base_location) <= proximity_threshold ||
                    Distance2D(enemy.pos, satellite_location) <= proximity_threshold - 10);
        });

    // Scout Marine Logic
    if (scout_marine_id != 0) {
        const sc2::Unit *scout_marine = Observation()->GetUnit(scout_marine_id);

        if (scout_marine) {
            // Ensure the scout Marine is actively scouting
            if (scout_marine->orders.empty()) {
                if (current_target_index < potential_enemy_locations_.size()) {
                    Actions()->UnitCommand(
                        scout_marine, ABILITY_ID::MOVE_MOVE,
                        potential_enemy_locations_[current_target_index]);
                    current_target_index = (current_target_index + 1) % potential_enemy_locations_.size();
                }
            }
        } else {
            // If the scout dies, log the enemy location and stop scouting
            if (current_target_index < potential_enemy_locations_.size()) {
                enemy_base_location = potential_enemy_locations_[current_target_index];
                potential_enemy_locations_.clear(); // Stop further scouting
            }
        }
    } else {
        // Assign a scout Marine if one hasn't been set yet
        Units marines = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_MARINE));
        for (const auto &marine : marines) {
            if (marine->orders.empty()) {
                scout_marine_id = marine->tag;
                break;
            }
        }
    }

    // Handle Nearby Enemies
    if (!enemies_near_base.empty()) {
        sc2::Point2D enemy_position = enemies_near_base.front()->pos;

        // Command all troops (excluding the scout Marine) to attack enemies
        Units marines = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_MARINE));
        for (const auto &marine : marines) {
            if (marine->orders.empty() && marine->tag != scout_marine_id) {
                Actions()->UnitCommand(marine, ABILITY_ID::ATTACK_ATTACK, enemy_position);
            }
        }

        Units tanks = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_SIEGETANK));
        for (const auto &tank : tanks) {
            if (tank->orders.empty()) {
                Actions()->UnitCommand(tank, ABILITY_ID::ATTACK_ATTACK, enemy_position);
            }
        }

        Units medivacs = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_MEDIVAC));
        for (const auto &medivac : medivacs) {
            if (medivac->orders.empty() && !marines.empty()) {
                Actions()->UnitCommand(medivac, ABILITY_ID::SMART, marines.front());
            }
        }

        Units vikings = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_VIKINGFIGHTER));
        for (const auto &viking : vikings) {
            if (viking->orders.empty()) {
                Actions()->UnitCommand(viking, ABILITY_ID::ATTACK_ATTACK, enemy_position);
            }
        }

        return; // Exit after issuing attack commands
    }

    // Rally Non-Scout Marines
    Units marines = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_MARINE));
    for (const auto &marine : marines) {
        if (marine->orders.empty() && marine->tag != scout_marine_id && !is_attacking) {
            Actions()->UnitCommand(marine, ABILITY_ID::MOVE_MOVE, rally_point);
        }
    }

    // Move Tanks to Rally Point
    Units tanks = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_SIEGETANK));
    for (const auto &tank : tanks) {
        if (tank->orders.empty() && !is_attacking) {
            Actions()->UnitCommand(tank, ABILITY_ID::MOVE_MOVE, rally_point);
        }
    }

    // Medivac Behavior
    Units medivacs = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_MEDIVAC));
    if (!HasSupportableGroundUnits()) {
        for (const auto &medivac : medivacs) {
            if (medivac->orders.empty()) {
                Actions()->UnitCommand(medivac, ABILITY_ID::MOVE_MOVE, GetBaseLocation());
            }
        }
    } else {
        for (const auto &medivac : medivacs) {
            if (medivac->orders.empty()) {
                if (!marines.empty()) {
                    Actions()->UnitCommand(medivac, ABILITY_ID::SMART, marines.front());
                }
            }
        }
    }

    // Vikings to Rally Point
    Units vikings = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_VIKINGFIGHTER));
    for (const auto &viking : vikings) {
        if (viking->orders.empty()) {
            Actions()->UnitCommand(viking, ABILITY_ID::MOVE_MOVE, rally_point);
        }
    }
}






// MODULAR UNIT TRAINING FUNCTIONS
void BasicSc2Bot::ManageCC() {
  Units cc = Observation()->GetUnits(
      // If satellite base upgraded to Orbital Command, then the same must be
      // done to the main base.
      Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_COMMANDCENTER));
  if (CountUnits(UNIT_TYPEID::TERRAN_ORBITALCOMMAND) > 0) {
    Actions()->UnitCommand(cc, ABILITY_ID::MORPH_ORBITALCOMMAND);
  }
}

void BasicSc2Bot::ManageBarracks() {
  const ObservationInterface *observation = Observation();
  Units barracks = Observation()->GetUnits(
      Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_BARRACKS));

  if (satellite_built) {
    for (const auto &barrack : barracks) {
      // Check if the Barracks has a Tech Lab
      const Unit *add_on = Observation()->GetUnit(barrack->add_on_tag);

          // Build a Tech Lab if no add-on exists and we don't already have one
      if (barrack->add_on_tag == NullTag && CountUnits(UNIT_TYPEID::TERRAN_BARRACKSTECHLAB) < 1) {
        Actions()->UnitCommand(barrack, ABILITY_ID::BUILD_TECHLAB);
        break; // We only want one Tech Lab, so stop after issuing the command
      }

      if (barrack->add_on_tag == NullTag &&
          (CountUnits(UNIT_TYPEID::TERRAN_BARRACKSTECHLAB) > 0) &&
          CountUnits(UNIT_TYPEID::TERRAN_REACTOR) < 2) {
        // Build a Reactor if no add-on exists and we don't already have one, but no more than two
        Actions()->UnitCommand(barrack, ABILITY_ID::BUILD_REACTOR);
        break; // We only want one Reactor to be built at a time.
      }

      // Research Combat Shields if Stimpack is done and Shields aren't done
      if (add_on &&
          add_on->unit_type == sc2::UNIT_TYPEID::TERRAN_BARRACKSTECHLAB &&
          !HasUpgrade(observation, UPGRADE_ID::SHIELDWALL)) {
        // Send the Combat Shields research command to the Tech Lab
        Actions()->UnitCommand(add_on, sc2::ABILITY_ID::RESEARCH_COMBATSHIELD);
      }

      // Research Stimpack if a Tech Lab is present and Stimpack isn't done
      if (add_on &&
          add_on->unit_type == sc2::UNIT_TYPEID::TERRAN_BARRACKSTECHLAB &&
          !HasUpgrade(observation, UPGRADE_ID::STIMPACK) &&
          HasUpgrade(observation, UPGRADE_ID::SHIELDWALL)) {
        // Send the Stimpack research command to the Tech Lab
        Actions()->UnitCommand(add_on, sc2::ABILITY_ID::RESEARCH_STIMPACK);
      }

      if (CountUnits(UNIT_TYPEID::TERRAN_ORBITALCOMMAND) > 0 &&
          CountUnits(UNIT_TYPEID::TERRAN_MARINE) < 48) {
        // Train Marines if both upgrades are done
        Actions()->UnitCommand(barrack, ABILITY_ID::TRAIN_MARINE);
      } else if (CountUnits(UNIT_TYPEID::TERRAN_ORBITALCOMMAND) < 1 &&
                 CountUnits(UNIT_TYPEID::TERRAN_MARINE) < 8) {
        // Train 8 Marines to repel early poke before Orbital Command
        Actions()->UnitCommand(barrack, ABILITY_ID::TRAIN_MARINE);
      } else if (CountUnits(UNIT_TYPEID::TERRAN_BARRACKS) > 2 &&
                CountUnits(UNIT_TYPEID::TERRAN_MARINE) < 48) {
        Actions()->UnitCommand(barrack, ABILITY_ID::TRAIN_MARINE);
      }
    }
  }
}

void BasicSc2Bot::ManageFactory() {
  /*
  Add tech lab add-on immediately if there is none.
  Train Siege Tanks until we have 6 such units.
  */
  Units factories = Observation()->GetUnits(
      Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_FACTORY));

  for (const auto &factory : factories) {
    const Unit *add_on = Observation()->GetUnit(factory->add_on_tag);
    if (factory->add_on_tag == NullTag) {
      // If Factory has no add-on, add a tech lab.
      Actions()->UnitCommand(factory, ABILITY_ID::BUILD_TECHLAB);
    } else if (add_on &&
               add_on->unit_type == UNIT_TYPEID::TERRAN_FACTORYTECHLAB) {
      // If Factory has a Tech Lab, order it to produce Hellions and Siege
      // Tanks

      // Produce Siege Tanks if fewer than 6
      if (CountUnits(UNIT_TYPEID::TERRAN_SIEGETANK) < 6) {
        Actions()->UnitCommand(factory, ABILITY_ID::TRAIN_SIEGETANK);
      }
    }
  }
}

void BasicSc2Bot::ManageStarport() {
  /*
  Training Pipeline:
  1) Starport MUST have Tech Lab to make a Banshee
    - If a Starport does not have Tech Lab, we order it to build one.
  2) Eligible Starports will train Banshees
  3) Eligible Starports will train 3 Medivac and then 5 Vikings are created.
  */

  Units starports = Observation()->GetUnits(
      Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_STARPORT));

  for (const auto &starport : starports) {
    const Unit *add_on = Observation()->GetUnit(starport->add_on_tag);
    if (starport->add_on_tag == NullTag) { // Check if Starport has no add-on
                                           // i.e. there is no tech lab
      Actions()->UnitCommand(starport, ABILITY_ID::BUILD_TECHLAB);
    } else if (add_on &&
               add_on->unit_type == UNIT_TYPEID::TERRAN_STARPORTTECHLAB &&
               starport->orders.empty()) {

      // make sure we have at least 1 medivac before we start making vikings,
      // and we have at least 4 vikings before we start making more medivacs
      if (CountUnits(UNIT_TYPEID::TERRAN_MEDIVAC) < 3) {
        Actions()->UnitCommand(starport, ABILITY_ID::TRAIN_MEDIVAC);
      }
    }
  }
}

void BasicSc2Bot::ManageSecondBase() {
  // Turn second CC into Orbital Command
  // (1) - If the second base is not morphed into an Orbital Command, do so
  // immediately. (2) - If the second base is an Orbital Command, then train
  // MULES, SCVs
  Units ccs = Observation()->GetUnits(
      Unit::Alliance::Self,
      IsUnit(UNIT_TYPEID::TERRAN_COMMANDCENTER)); // Get list of all CCs on
                                                  // our team
  if (CountUnits(UNIT_TYPEID::TERRAN_ORBITALCOMMAND) < 1) {
    for (const auto &cc : ccs) {
      if (Point2D(cc->pos) != base_location) {
        if (cc->orders.empty()) {
          Actions()->UnitCommand(
              cc,
              ABILITY_ID::MORPH_ORBITALCOMMAND); // Change to orbital command
          satellite_built = true;
        }
      }
    }
  }
}

void BasicSc2Bot::ManageEnggBay() {
  if (CountUnits(UNIT_TYPEID::TERRAN_ENGINEERINGBAY) > 0) {
    const Unit* eb = Observation()->GetUnits(Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_ENGINEERINGBAY)).front();
    const ObservationInterface *observation = Observation();
    if (!HasUpgrade(observation, UPGRADE_ID::TERRANINFANTRYWEAPONSLEVEL1)) {
      // Upgrade to Infantry Weapons Level I (impacts Marines)
      Actions()->UnitCommand(eb, ABILITY_ID::RESEARCH_TERRANINFANTRYWEAPONSLEVEL1);
    }
  } 
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

const Unit *BasicSc2Bot::FindNearestVespene(const Point2D &start) {
  Units units = Observation()->GetUnits(Unit::Alliance::Neutral);
  float distance = std::numeric_limits<float>::max();
  const Unit *target = nullptr;
  for (const auto &u : units) {
    if (u->unit_type == UNIT_TYPEID::NEUTRAL_VESPENEGEYSER ||
        u->unit_type == UNIT_TYPEID::NEUTRAL_RICHVESPENEGEYSER) {
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

int BasicSc2Bot::MilitaryStrength() {
  int marines = CountUnits(UNIT_TYPEID::TERRAN_MARINE);
  int medivacs = CountUnits(UNIT_TYPEID::TERRAN_MEDIVAC);
  int tanks = CountUnits(UNIT_TYPEID::TERRAN_SIEGETANK);

  bool can_attack = false;

  // if we have 45 marines, 6 siege tanks, and 2 medivacs
  if (marines >= 40 && tanks >= 6 && medivacs >= 2) {
    can_attack = true;
  }

  return can_attack;
}

// Function to get the location fo the commance center
Point2D BasicSc2Bot::GetBaseLocation() {
  if (base_location == Point2D()) {
    const ObservationInterface *observation = Observation();
    const Unit *command_center =
        observation
            ->GetUnits(Unit::Alliance::Self,
                       IsUnit(UNIT_TYPEID::TERRAN_COMMANDCENTER))
            .front();
    base_location =
        command_center->pos; // Set base location if not already set.
  }
  return base_location;
}

bool BasicSc2Bot::HasUpgrade(const sc2::ObservationInterface *observation,
                             sc2::UpgradeID upgrade_id) {
  const auto &upgrades = observation->GetUpgrades();
  return std::find(upgrades.begin(), upgrades.end(), upgrade_id) !=
         upgrades.end();
}

void BasicSc2Bot::InitializeSatelliteLocation() {
    const ObservationInterface *observation = Observation();
    QueryInterface *query = Query();

    // Set up parameters for expansion calculations
    sc2::search::ExpansionParameters parameters;
    parameters.cluster_distance_ = 10.0f; // Distance to cluster resources
    parameters.circle_step_size_ = 2.0f;  // Step size for placement queries
    parameters.radiuses_ = {6.0f};        // Radius for base placement
    parameters.debug_ = nullptr;         // Debug interface (optional)

    // Calculate potential expansion locations
    std::vector<Point3D> expansion_locations = CalculateExpansionLocations(observation, query, parameters);

    const float MIN_DISTANCE = 11.0f; // Minimum distance from current base
    const float MAX_DISTANCE = 40.0f; // Maximum distance for expansion

    // Find the best expansion location
    float best_score = std::numeric_limits<float>::max();
    Point3D best_location;

    for (const auto &location : expansion_locations) {
        float distance_from_base = Distance2D(Point2D(location), base_location);

        // Skip locations too close or too far
        if (distance_from_base < MIN_DISTANCE || distance_from_base > MAX_DISTANCE) {
            continue;
        }

        // Score the location based on its proximity to the midpoint between MIN_DISTANCE and MAX_DISTANCE
        float ideal_distance = (MIN_DISTANCE + MAX_DISTANCE) / 2.0f;
        float score = std::abs(distance_from_base - ideal_distance);

        if (score < best_score) {
            best_score = score;
            best_location = location;
        }
    }

    // Update satellite location if a valid one was found
    if (best_score < std::numeric_limits<float>::max()) {
        satellite_location = Point2D(best_location.x, best_location.y);
    }
}
