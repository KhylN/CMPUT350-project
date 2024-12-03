#include "BasicSc2Bot.h"
using namespace sc2;

void BasicSc2Bot::InitializeEnemyLocations() {
  const sc2::GameInfo &game_info = Observation()->GetGameInfo();
  for (const sc2::Point2D &starting_location :
       game_info.enemy_start_locations) {
    potential_enemy_locations_.push_back(starting_location);
  }
  // output the list of potential enemy locations
  std::cout << "Potential Enemy Locations: " << std::endl;
  for (const sc2::Point2D &location : potential_enemy_locations_) {
    std::cout << location.x << ", " << location.y << std::endl;
  }
}

void BasicSc2Bot::OnGameStart() {
  // Initialize the enemy locations
  InitializeEnemyLocations();
  GetBaseLocation();
  std::cout << "Go!" << std::endl;
}

void BasicSc2Bot::OnStep() {
  ManageTroopsAndBuildings();
  std::cout << "Orbital command flying: "
            << CountUnits(UNIT_TYPEID::TERRAN_ORBITALCOMMANDFLYING) << std::endl
            << std::endl
            << std::endl
            << std::endl;
  std::cout << "main_base_mineral_patch: " << main_base_mineral_patch.x << ", "
            << main_base_mineral_patch.y << std::endl
            << std::endl
            << std::endl
            << std::endl;
  std::cout << "Satellite Location: " << satellite_location.x << ", "
            << satellite_location.y << std::endl
            << std::endl
            << std::endl
            << std::endl;

  // TODO: Expanded Logic for Posture II
}

// Will run every time a unit is idle
void BasicSc2Bot::OnUnitIdle(const sc2::Unit *unit) {
  switch (unit->unit_type.ToType()) {
  case UNIT_TYPEID::TERRAN_COMMANDCENTER: {
    if (CountUnits(UNIT_TYPEID::TERRAN_ORBITALCOMMAND) > 0) {
      // Prioritize building new Orbital Command if we do not have one.
      Actions()->UnitCommand(unit, ABILITY_ID::MORPH_ORBITALCOMMAND);
      break;
    } else if (CountUnits(UNIT_TYPEID::TERRAN_SCV) < 30 &&
      // Build SCVs all other times.
        Point2D(unit->pos) == base_location) {
      Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_SCV);
    }
    break;
  }
  case UNIT_TYPEID::TERRAN_ORBITALCOMMAND: {
    if (CountUnits(UNIT_TYPEID::TERRAN_MULE) < 4) {
      Actions()->UnitCommand(unit, ABILITY_ID::EFFECT_CALLDOWNMULE, FindNearestMineralPatch(satellite_location));
    }
    if (CountUnits(UNIT_TYPEID::TERRAN_SCV) < 60) {
      Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_SCV);
    }
    break;
  }

  case UNIT_TYPEID::TERRAN_SCV: {
    // TODO: Try to prioritize some SCVs to find gas as opposed to mineral.
    const Unit *mineral_target = FindNearestMineralPatch(unit->pos);
    main_base_mineral_patch = Point2D(mineral_target->pos);
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
    Point2D second_base_mineral_patch = Point2D(mineral_target->pos);

    if (mineral_target) {
      Actions()->UnitCommand(unit, ABILITY_ID::SMART, mineral_target);
    }
    break;
  }
  case UNIT_TYPEID::TERRAN_MARINE: {
    // Static variables to track the scout Marine's ID and discovered enemy base
    // location

    // Get all Marines controlled by the player
    Units marines = Observation()->GetUnits(Unit::Alliance::Self,
                                            IsUnit(UNIT_TYPEID::TERRAN_MARINE));
    const GameInfo &game_info = Observation()->GetGameInfo();

    // Check if there are any enemy start locations
    if (game_info.enemy_start_locations.empty()) {
      break; // No enemy start locations to scout
    }

    // if (scout_died) {
    //   // If the scout Marine is dead, ensure all other Marines are patrolling
    //   for (const auto &marine : marines) {
    //     if (marine->orders.empty()) {
    //       Actions()->UnitCommand(marine, ABILITY_ID::GENERAL_PATROL,
    //                              GetBaseLocation());
    //     }
    //   }
    //   break; // Exit the case since no further scouting is needed
    // }

    for (const auto &marine : marines) {
      if (scout_marine_id == 0) {
        // Assign the first idle Marine as the scout if no scout is designated
        if (marine->orders.empty()) {
          scout_marine_id = marine->tag; // Assign this Marine as the scout
          Actions()->UnitCommand(
              marine, ABILITY_ID::ATTACK_ATTACK,
              game_info.enemy_start_locations[current_target_index]);
        }
      } else if (marine->tag == scout_marine_id) {
        // If this Marine is already the scout, ensure it continues scouting
        if (marine->orders.empty()) {
          // Move to the next target location if the scout has reached the
          // current one
          current_target_index = (current_target_index + 1) %
                                 game_info.enemy_start_locations.size();
          Actions()->UnitCommand(
              marine, ABILITY_ID::ATTACK_ATTACK,
              game_info.enemy_start_locations[current_target_index]);
        }
      } else {
        // Other idle Marines should patrol
        if (marine->orders.empty()) {
          if (satellite_location.x != 0 && satellite_location.y != 0) {
            Actions()->UnitCommand(marine, ABILITY_ID::GENERAL_PATROL,
                                   satellite_location);
          } else {
            Actions()->UnitCommand(marine, ABILITY_ID::GENERAL_PATROL,
                                   GetBaseLocation());
          }
        }
      }
    }

    // Check if the scout Marine is alive
    bool scout_is_alive = false;
    for (const auto &marine : marines) {
      if (marine->tag == scout_marine_id) {
        scout_is_alive = true;
        break;
      }
    }

    if (!scout_is_alive && scout_marine_id != 0) {
      // If the scout Marine is dead, mark the scout as dead
      scout_died = true;
      // set the enemy base location
      enemy_base_location =
          game_info.enemy_start_locations[current_target_index];
      std::cout << "Enemy base location identified: " << enemy_base_location.x
                << ", " << enemy_base_location.y << std::endl;
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
        // Only select idle SCVs or those not assigned to other tasks
        for (const auto &order : scv->orders) {
          if (order.ability_id == ABILITY_ID::BUILD_SUPPLYDEPOT ||
            order.ability_id == ABILITY_ID::BUILD_BARRACKS ||
            order.ability_id == ABILITY_ID::BUILD_FACTORY ||
            order.ability_id == ABILITY_ID::BUILD_STARPORT ||
            order.ability_id == ABILITY_ID::BUILD_REFINERY ||
            order.ability_id == ABILITY_ID::BUILD_COMMANDCENTER) {
            continue;
          } else {
            Actions()->UnitCommand(scv, ABILITY_ID::SMART, refinery);
            ++scv_count;
            break;
          }
        }
      }
    } else if (scv_count > 3) {
      // Unassign extra SCVs if more than 3 are assigned
      for (const auto &scv : scvs) {
        if (!scv->orders.empty() &&
            scv->orders.front().target_unit_tag == refinery->tag) {
          Actions()->UnitCommand(scv, ABILITY_ID::STOP);
          --scv_count;
          if (scv_count == 3) {
            break;
          }
        }
      }
    }
  }
}

void BasicSc2Bot::ManageTroopsAndBuildings() {
  TryBuildRefinery();
  ManageSCVs();

  TryBuildNewCC();
  ManageSecondBase();

  TryBuildEnggBay();

  TryBuildSupplyDepot();
  TryMorphSupplyDepot();
  TryBuildBarracks();
  ManageBarracks();

  // TryBuildEnggBay();
  // TryBuildMissileTurret();

  TryBuildStarport();
  ManageStarport();

  TryBuildFactory();
  ManageFactory();

  int strength = MilitaryStrength();
  std::cout << "Military Strength: " << strength << std::endl;
  std::cout << "Marines: " << CountUnits(UNIT_TYPEID::TERRAN_MARINE)
            << " Hellions: " << CountUnits(UNIT_TYPEID::TERRAN_HELLION)
            << " Vikings: " << CountUnits(UNIT_TYPEID::TERRAN_VIKINGFIGHTER)
            << " Medivacs: " << CountUnits(UNIT_TYPEID::TERRAN_MEDIVAC)
            << " Tanks: " << CountUnits(UNIT_TYPEID::TERRAN_SIEGETANK)
            << std::endl;

  if (strength > THRESH) {
    std::cout << "Launching attack! Wave: " << current_attack_wave_
              << std::endl;
    LaunchAttack();
  } else {
    std::cout << "Not strong enough to attack yet. Current: " << strength
              << " Threshold: " << THRESH << std::endl;
    ManageAllTroops();
  }
}

void BasicSc2Bot::LaunchAttack() {

  std::cout << "Potential Enemy Locations: " << std::endl;
  for (const sc2::Point2D &location : potential_enemy_locations_) {
    std::cout << location.x << ", " << location.y << std::endl;
  }

  std::cout << "enemy base location: " << enemy_base_location.x << ", "
            << enemy_base_location.y << std::endl;

  SendArmyTo(enemy_base_location);
}

bool BasicSc2Bot::IsArmyIdle() {
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

  std::cout << "Checking if army is idle" << std::endl;

  // output the orders of each unit
  for (const auto &marine : marines) {
    for (const auto &order : marine->orders) {
      std::cout << "Marine order: " << order.ability_id << std::endl;
    }
  }

  for (const auto &marine : marines) {
    if (!marine->orders.empty()) {
      return false;
    }
  }
  for (const auto &hellion : hellions) {
    if (!hellion->orders.empty()) {
      return false;
    }
  }
  for (const auto &tank : tanks) {
    if (!tank->orders.empty()) {
      return false;
    }
  }
  for (const auto &medivac : medivacs) {
    if (!medivac->orders.empty()) {
      return false;
    }
  }
  for (const auto &viking : vikings) {
    if (!viking->orders.empty()) {
      return false;
    }
  }

  return true;
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
    Actions()->UnitCommand(tank, sc2::ABILITY_ID::ATTACK_ATTACK, target);
  }

  // Send ground units
  for (const auto &unit : ground_units) {
    Actions()->UnitCommand(unit, sc2::ABILITY_ID::ATTACK_ATTACK, target);
  }
  for (const auto &hellion : hellions) {
    Actions()->UnitCommand(hellion, sc2::ABILITY_ID::ATTACK_ATTACK, target);
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
      Actions()->UnitCommand(medivac, sc2::ABILITY_ID::MOVE_MOVE,
                             GetBaseLocation());
    }
  }

  // Send vikings last
  for (const auto &viking : vikings) {
    Actions()->UnitCommand(viking, sc2::ABILITY_ID::ATTACK_ATTACK, target);
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
          order.ability_id == ABILITY_ID::BUILD_COMMANDCENTER) {
        unit_is_busy = true;
        break;
      }
    }

    // Select an idle unit that matches the required type
    if (!unit_is_busy) {
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
    build_location = FindBuildLocation(base_location, ability_type_for_structure, 4);
  } else if (ability_type_for_structure == ABILITY_ID::BUILD_COMMANDCENTER) {
    if (satellite_location != Point2D()) {
      build_location = satellite_location;
    } else {
      return false;
    }
  } 
  else {
    build_location =
        FindBuildLocation(base_location, ability_type_for_structure);
  }

  if (build_location ==
      Point2D()) { // Check if FindBuildLocation returned a valid location
    std::cerr << "No valid build location found for structure: "
              << static_cast<int>(ability_type_for_structure) << std::endl;
    return false;
  } else {
    std::cout << "Build location found at: (" << build_location.x << ", "
              << build_location.y << ")" << std::endl;
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
  std::cout << "Issued build command to unit: " << unit_to_build->tag
            << " for structure: "
            << static_cast<int>(ability_type_for_structure) << " at location: ("
            << build_location.x << ", " << build_location.y << ")" << std::endl;

  return true;
}

Point2D BasicSc2Bot::FindBuildLocation(Point2D base_location,
                                       ABILITY_ID ability_type,
                                       float build_radius) {
  const ObservationInterface *observation =
      Observation(); // Ensure we use the observation interface
  // QueryInterface *query = Actions()->GetQueryInterface(); // Explicitly get
  // the query interface

  for (float dx = -build_radius; dx <= build_radius; dx += 3.0f) {
    for (float dy = -build_radius; dy <= build_radius; dy += 3.0f) {
      Point2D current_location =
          Point2D(base_location.x + dx, base_location.y + dy);

      // Use query->Placement() instead of Query()->Placement() if needed
      if (Query()->Placement(ability_type, current_location)) {
        // Ensure sufficient space for tech lab if necessary
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

  if (CountUnits(UNIT_TYPEID::TERRAN_REFINERY) < 1 ||
      (CountUnits(UNIT_TYPEID::TERRAN_ORBITALCOMMAND) > 0 &&
       CountUnits(UNIT_TYPEID::TERRAN_REFINERY) < 2)) {
    for (const auto &geyser : geysers) {
      // Check distance from the base
      float distance = Distance2D(geyser->pos, base_location);
      if (distance < 20.0f) { // Only consider geysers within 20 units
        for (const auto &scv : scvs) {
          // Only select SCVs that are not currently harvesting or building
          if (scv->orders.empty() ||
              (scv->orders.front().ability_id != ABILITY_ID::HARVEST_GATHER &&
               scv->orders.front().ability_id != ABILITY_ID::HARVEST_RETURN)) {
            Actions()->UnitCommand(scv, ABILITY_ID::BUILD_REFINERY, geyser);
            break;
          }
        }
      }
    }
  } else if (CountUnits(UNIT_TYPEID::TERRAN_REFINERY) > 4 && CountUnits(UNIT_TYPEID::TERRAN_ORBITALCOMMAND) > 0) {
    for (const auto &geyser : geysers) {
      // Check distance from the SATELLITE base
      float distance = Distance2D(geyser->pos, satellite_location);
      if (distance < 20.0f) { // Only consider geysers within 20 units of the satellite base
        for (const auto &scv : scvs) {
          // Only select SCVs that are not currently harvesting or building
          if (scv->orders.empty() ||
              (scv->orders.front().ability_id != ABILITY_ID::HARVEST_GATHER &&
               scv->orders.front().ability_id != ABILITY_ID::HARVEST_RETURN)) {
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
  if (observation->GetFoodUsed() > observation->GetFoodCap() - 2) {
        return TryBuildStructure(ABILITY_ID::BUILD_SUPPLYDEPOT);
      } // Build Supply Depot IFF we have already built the 2nd Command Center
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
  if (CountUnits(UNIT_TYPEID::TERRAN_BARRACKS) < 1) {
    return TryBuildStructure(ABILITY_ID::BUILD_BARRACKS);
  } 
  return false;
}

bool BasicSc2Bot::TryBuildNewCC() {
  // Build satellite command center to facilitate more eco. Built immediately
  // after 1st Vespene
  if (CountUnits(UNIT_TYPEID::TERRAN_REFINERY) > 0 && !satellite_built) {
    Units mineral_patches = Observation()->GetUnits(
            Unit::Alliance::Neutral, [](const Unit &unit) {
              return unit.unit_type == UNIT_TYPEID::NEUTRAL_MINERALFIELD ||
                     unit.unit_type == UNIT_TYPEID::NEUTRAL_MINERALFIELD750;
            });

    // Get current base mineral patches (those being harvested)
    const float MIN_DISTANCE = 20.0f; // Minimum distance from current base
    const float MAX_DISTANCE = 30.0f; // Maximum distance for expansion

    // Find a mineral patch that's at an appropriate distance
    const Unit *new_mineral_target = nullptr;
    float best_distance = std::numeric_limits<float>::max();

    for (const auto &mineral : mineral_patches) {
      float dist_from_current =
          Distance2D(Point2D(mineral->pos), base_location);

      // Skip if too close or too far
      if (dist_from_current < MIN_DISTANCE ||
          dist_from_current > MAX_DISTANCE) {
        continue;
      }

      // Find the mineral patch closest to our ideal distance
      // (which is halfway between min and max)
      float ideal_distance = (MIN_DISTANCE + MAX_DISTANCE) / 2;
      float distance_from_ideal =
          std::abs(dist_from_current - ideal_distance);

      if (distance_from_ideal < best_distance) {
        best_distance = distance_from_ideal;
        new_mineral_target = mineral;
      }
    }

    if (new_mineral_target) {
      Point2D mineral_target_pos = Point2D(new_mineral_target->pos);
      std::cout << "New Mineral Target Pos: " << mineral_target_pos.x
                << ", " << mineral_target_pos.y << std::endl;

      satellite_location = FindBuildLocation(
          mineral_target_pos, ABILITY_ID::BUILD_COMMANDCENTER, 6.0f);
      std::cout << "New Satellite Location: " << satellite_location.x
                << ", " << satellite_location.y << std::endl;

      return TryBuildStructure(ABILITY_ID::BUILD_COMMANDCENTER);
    }
  }
  return false;
}

bool BasicSc2Bot::TryBuildEnggBay() {
  // Build Engineering Bay if we have built or satellite base.
  // Check if all barracks have a tech lab
  if (CountUnits(UNIT_TYPEID::TERRAN_ENGINEERINGBAY) < 1 &&
      CountUnits(UNIT_TYPEID::TERRAN_ORBITALCOMMAND) > 0) {
    return TryBuildStructure(ABILITY_ID::BUILD_ENGINEERINGBAY);
  }
  return false;
}

bool BasicSc2Bot::TryBuildMissileTurret() {
  // Build MissileTurret if we have an Engineering Bay (prereq) and if we have
  // no more than 3.
  if (CountUnits(UNIT_TYPEID::TERRAN_ENGINEERINGBAY) > 0 &&
      CountUnits(UNIT_TYPEID::TERRAN_BARRACKS) > 1 &&
      CountUnits(UNIT_TYPEID::TERRAN_MISSILETURRET) < 3) {
    return TryBuildStructure(ABILITY_ID::BUILD_MISSILETURRET);
  }
  return false;
}

bool BasicSc2Bot::TryBuildStarport() {
  // Build Starport if we don't have one and if we have a Factory.
  if (CountUnits(UNIT_TYPEID::TERRAN_STARPORT) < 2 &&
      CountUnits(UNIT_TYPEID::TERRAN_REFINERY) > 3 {
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

  // SCOUTING

  // patrol command for Marauders
  Units marauders = Observation()->GetUnits(
      Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_MARAUDER));

  for (const auto &marauder : marauders) {
    if (marauder->orders.empty()) { // If marauder is idle
      if (satellite_location.x != 0 && satellite_location.y != 0) {
        Actions()->UnitCommand(marauder, ABILITY_ID::GENERAL_PATROL,
                               satellite_location);
      }
    } else {
      Actions()->UnitCommand(marauder, ABILITY_ID::GENERAL_PATROL,
                             GetBaseLocation());
    }
  }

  // patrol command for Hellions
  Units hellions = Observation()->GetUnits(Unit::Alliance::Self,
                                           IsUnit(UNIT_TYPEID::TERRAN_HELLION));

  for (const auto &hellion : hellions) {
    if (hellion->orders.empty()) {
      if (satellite_location.x != 0 && satellite_location.y != 0) {
        Actions()->UnitCommand(hellion, ABILITY_ID::GENERAL_PATROL,
                               satellite_location);
      } else {
        Actions()->UnitCommand(hellion, ABILITY_ID::GENERAL_PATROL,
                               GetBaseLocation());
      }
    }
  }

  // patrol command for Siege Tanks
  Units siegeTanks = Observation()->GetUnits(
      Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_SIEGETANK));
  for (const auto &tank : siegeTanks) {
    if (tank->orders.empty()) {
      if (satellite_location.x != 0 && satellite_location.y != 0) {
        Actions()->UnitCommand(tank, ABILITY_ID::GENERAL_PATROL,
                               satellite_location);
      } else {
        Actions()->UnitCommand(tank, ABILITY_ID::GENERAL_PATROL,
                               GetBaseLocation());
      }
    }
  }

  // patrol command for Medivacs
  Units medivacs = Observation()->GetUnits(Unit::Alliance::Self,
                                           IsUnit(UNIT_TYPEID::TERRAN_MEDIVAC));

  if (!HasSupportableGroundUnits()) {
    // If no ground units to support, return medivacs to base
    for (const auto &medivac : medivacs) {
      if (medivac->orders.empty()) {
        Actions()->UnitCommand(medivac, ABILITY_ID::MOVE_MOVE,
                               GetBaseLocation());
      }
    }
  } else {
    // Find nearest ground unit to support if idle
    for (const auto &medivac : medivacs) {
      if (medivac->orders.empty()) {
        Units marines = Observation()->GetUnits(
            Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_MARINE));
        Units marauders = Observation()->GetUnits(
            Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_MARAUDER));

        float closest_distance = std::numeric_limits<float>::max();
        const Unit *closest_unit = nullptr;

        // Find closest marine or marauder
        for (const auto &marine : marines) {
          float distance = DistanceSquared2D(medivac->pos, marine->pos);
          if (distance < closest_distance) {
            closest_distance = distance;
            closest_unit = marine;
          }
        }

        for (const auto &marauder : marauders) {
          float distance = DistanceSquared2D(medivac->pos, marauder->pos);
          if (distance < closest_distance) {
            closest_distance = distance;
            closest_unit = marauder;
          }
        }

        if (closest_unit) {
          Actions()->UnitCommand(medivac, ABILITY_ID::SMART, closest_unit);
        }
      }
    }
  }

  // patrol command for Vikings
  Units vikings = Observation()->GetUnits(
      Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_VIKINGFIGHTER));
  for (const auto &viking : vikings) {
    if (viking->orders.empty()) {
      if (satellite_location.x != 0 && satellite_location.y != 0) {
        Actions()->UnitCommand(viking, ABILITY_ID::GENERAL_PATROL,
                               satellite_location);
      } else {
        Actions()->UnitCommand(viking, ABILITY_ID::GENERAL_PATROL,
                               GetBaseLocation());
      }
    }
  }
}

// MODULAR UNIT TRAINING FUNCTIONS

void BasicSc2Bot::ManageBarracks() {
  Units barracks = Observation()->GetUnits(
      Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_BARRACKS));
  if (satellite_built) {
    for (const auto &barrack : barracks) {
      if (CountUnits(UNIT_TYPEID::TERRAN_MARINE) < 5) {
        Actions()->UnitCommand(barrack, ABILITY_ID::TRAIN_MARINE);
      } 
    }
  }
}

void BasicSc2Bot::ManageFactory() {
  /*
  Training Pipeline:
  1) Produce 5 Hellions first, then transition to Siege Tanks.
  2) Factory MUST have Tech Lab to make a Siege Tank
    - If a Starport does not have Tech Lab, we order it to build one.
  3) Eligible Starports will train Siege Tank
  */
  Units factories = Observation()->GetUnits(
      Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_FACTORY));

  for (const auto &factory : factories) {
    const Unit *add_on = Observation()->GetUnit(factory->add_on_tag);
    if (factory->add_on_tag == NullTag &&
        CountUnits(UNIT_TYPEID::TERRAN_REFINERY) > 3) {
      // If Factory has no add-on AND we have the 4 refineries, order it to build a Tech Lab
      Actions()->UnitCommand(factory, ABILITY_ID::BUILD_TECHLAB);
    } else if (add_on &&
               add_on->unit_type == UNIT_TYPEID::TERRAN_FACTORYTECHLAB) {
      // If Factory has a Tech Lab, order it to produce Hellions and Siege
      // Tanks

      if (CountUnits(UNIT_TYPEID::TERRAN_SIEGETANK) >= 5
          factory->orders.empty()) {
        // Produce Hellions if fewer than 5 exist
        Actions()->UnitCommand(factory, ABILITY_ID::TRAIN_HELLION);
      } else {
        // Produce Siege Tanks if fewer than 5 exist
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
      if (CountUnits(UNIT_TYPEID::TERRAN_MEDIVAC) < 1) {
        Actions()->UnitCommand(starport, ABILITY_ID::TRAIN_MEDIVAC);
      } else if (CountUnits(UNIT_TYPEID::TERRAN_VIKINGFIGHTER) >= 4) {
        Actions()->UnitCommand(starport, ABILITY_ID::TRAIN_MEDIVAC);
      } else if (CountUnits(UNIT_TYPEID::TERRAN_VIKINGFIGHTER) < 4) {
        Actions()->UnitCommand(starport, ABILITY_ID::TRAIN_VIKINGFIGHTER);
      }
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
    if (u->unit_type == UNIT_TYPEID::NEUTRAL_VESPENEGEYSER) {
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
  int hellions = CountUnits(UNIT_TYPEID::TERRAN_HELLION);
  int vikings = CountUnits(UNIT_TYPEID::TERRAN_VIKINGFIGHTER);
  int medivacs = CountUnits(UNIT_TYPEID::TERRAN_MEDIVAC);
  int tanks = CountUnits(UNIT_TYPEID::TERRAN_SIEGETANK);

  // Weight each unit type based on their relative strength/importance
  return marines + (hellions * 2) + (vikings * 2) + medivacs + (tanks * 3);
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

void BasicSc2Bot::ManageSecondBase() {
  // Turn second CC into Orbital Command
  // (1) - If the second base is not morphed into an Orbital Command, do so immediately.
  // (2) - If the second base is an Orbital Command, then train MULES, SCVs
  Units ccs = Observation()->GetUnits(
      Unit::Alliance::Self,
      IsUnit(UNIT_TYPEID::TERRAN_COMMANDCENTER)); // Get list of all CCs on our
                                                  // team
  if (CountUnits(UNIT_TYPEID::TERRAN_ORBITALCOMMAND) < 1) {
    for (const auto &cc : ccs) {
      if (Point2D(cc->pos) != base_location) {
        if (cc->orders.empty()) {
          Actions()->UnitCommand(
              cc, ABILITY_ID::MORPH_ORBITALCOMMAND); // Change to orbital command
          satellite_built = true;
        }
      }
    }
  }
}