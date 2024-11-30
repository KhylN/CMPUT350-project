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
  std::cout << "Go!" << std::endl;
}

void BasicSc2Bot::OnStep() {
  ManageSCVs();
  ManageTroopsAndBuildings();

  // TODO: Expanded Logic for Posture II
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
    // TODO: Try to prioritize some SCVs to find gas as opposed to mineral.
    const Unit *mineral_target = FindNearestMineralPatch(unit->pos);
    const Unit *gas_target = FindNearestVespene(unit->pos);

    if (mineral_target) {
      Actions()->UnitCommand(unit, ABILITY_ID::SMART, mineral_target);
    }
    break;

    /*
    if (gas_target) {
      Actions()->UnitCommand(unit, ABILITY_ID::SMART, gas_target);
    }
    break;
    */
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

    if (scout_died) {
      // If the scout Marine is dead, ensure all other Marines are patrolling
      for (const auto &marine : marines) {
        if (marine->orders.empty()) {
          Actions()->UnitCommand(marine, ABILITY_ID::GENERAL_PATROL,
                                 GetBaseLocation());
        }
      }
      break; // Exit the case since no further scouting is needed
    }

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
          Actions()->UnitCommand(marine, ABILITY_ID::GENERAL_PATROL,
                                 GetBaseLocation());
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
  case UNIT_TYPEID::TERRAN_BARRACKS: {
    if (CountUnits(UNIT_TYPEID::TERRAN_MARINE) < 20) {
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
  Units refineries = observation->GetUnits(
      Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_REFINERY));
  Units geysers = observation->GetUnits(
      Unit::Alliance::Neutral, IsUnit(UNIT_TYPEID::NEUTRAL_VESPENEGEYSER));

  // Get command center position
  const Unit *command_center =
      observation
          ->GetUnits(Unit::Alliance::Self,
                     IsUnit(UNIT_TYPEID::TERRAN_COMMANDCENTER))
          .front();
  Point2D base_position = command_center->pos;

  // Build refineries if fewer than 2 exist
  if (CountUnits(UNIT_TYPEID::TERRAN_REFINERY) < 2) {
    for (const auto &geyser : geysers) {
      // Check distance from the base
      float distance = Distance2D(geyser->pos, base_position);
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
  }

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
        if (scv->orders.empty() ||
            (scv->orders.front().ability_id != ABILITY_ID::BUILD_REFINERY &&
             scv->orders.front().ability_id != ABILITY_ID::HARVEST_GATHER &&
             scv->orders.front().ability_id != ABILITY_ID::HARVEST_RETURN)) {
          Actions()->UnitCommand(scv, ABILITY_ID::SMART, refinery);
          ++scv_count;
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
  TryBuildSupplyDepot();
  TryBuildBarracks();
  ManageBarracks();

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
  // if (current_attack_wave_ >= potential_enemy_locations_.size()) {
  //   std::cout << "No more attack locations available" << std::endl;
  //   ManageAllTroops();
  //   return;
  // }

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

  sc2::Point2D rally_point = sc2::Point2D((target.x + GetBaseLocation().x) / 2,
                                          (target.y + GetBaseLocation().y) / 2);

  // Send tanks first
  for (const auto &tank : tanks) {
    Actions()->UnitCommand(tank, sc2::ABILITY_ID::ATTACK_ATTACK, target);
  }

  // Send ground units
  for (const auto &marine : marines) {
    Actions()->UnitCommand(marine, sc2::ABILITY_ID::ATTACK_ATTACK, target);
  }
  for (const auto &hellion : hellions) {
    Actions()->UnitCommand(hellion, sc2::ABILITY_ID::ATTACK_ATTACK, target);
  }

  // Send support units
  for (const auto &medivac : medivacs) {
    Actions()->UnitCommand(medivac, sc2::ABILITY_ID::ATTACK_ATTACK, target);
  }
  for (const auto &viking : vikings) {
    Actions()->UnitCommand(viking, sc2::ABILITY_ID::ATTACK_ATTACK, target);
  }
}

// Attempts to build specified structures - from tutorial
// Attempts to build specified structures - from tutorial
bool BasicSc2Bot::TryBuildStructure(ABILITY_ID ability_type_for_structure, UNIT_TYPEID unit_type) {
    const ObservationInterface *observation = Observation();
    
    // Find an available builder unit of the specified type
    const Unit *unit_to_build = nullptr;
    Units units = observation->GetUnits(Unit::Alliance::Self);
    for (const auto &unit : units) {

    for (const auto &order : unit->orders) {
      if (order.ability_id == ability_type_for_structure) {
        return false;
      }
    }
    if (unit->unit_type == unit_type && !unit->orders.empty()) {
      unit_to_build = unit;
    }
  }
  if (!unit_to_build) {
    return false;
  }
    // Find a valid build location near the base
    Point2D base_location = GetBaseLocation();
    Point2D build_location = FindBuildLocation(base_location, ability_type_for_structure);

    if (build_location == Point2D()) { // Check if FindBuildLocation returned a valid location
        std::cerr << "No valid build location found for structure: " << static_cast<int>(ability_type_for_structure) << std::endl;
        return false;
    } else {
        std::cout << "Build location found at: (" << build_location.x << ", " << build_location.y << ")" << std::endl;
    }

    // Check resources

    // Issue the build command
    Actions()->UnitCommand(unit_to_build, ability_type_for_structure, build_location);
    std::cout << "Issued build command to unit: " << unit_to_build->tag 
              << " for structure: " << static_cast<int>(ability_type_for_structure) 
              << " at location: (" << build_location.x << ", " << build_location.y << ")" << std::endl;

    return true;
}


Point2D BasicSc2Bot::FindBuildLocation(Point2D base_location, ABILITY_ID ability_type) {
    const ObservationInterface *observation = Observation(); // Ensure we use the observation interface
    //QueryInterface *query = Actions()->GetQueryInterface(); // Explicitly get the query interface
    
    float build_radius = 12.0f;

    for (float dx = -build_radius; dx <= build_radius; dx += 2.0f) {
        for (float dy = -build_radius; dy <= build_radius; dy += 2.0f) {
            Point2D current_location = Point2D(base_location.x + dx, base_location.y + dy);

            // Use query->Placement() instead of Query()->Placement() if needed
            if (Query()->Placement(ability_type, current_location)) {
                // Ensure sufficient space for tech lab if necessary
                if (ability_type == ABILITY_ID::BUILD_FACTORY || ability_type == ABILITY_ID::BUILD_STARPORT) {
                    if (Query()->Placement(ability_type, Point2D(current_location.x + 3.0f, current_location.y))) {
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
bool BasicSc2Bot::TryBuildSupplyDepot() {
  const ObservationInterface *observation = Observation();
  if (observation->GetFoodUsed() <= observation->GetFoodCap() - 2)
    return false;

  std::cout << "entered try build structure" << std::endl;
  return TryBuildStructure(ABILITY_ID::BUILD_SUPPLYDEPOT,
                           UNIT_TYPEID::TERRAN_SCV);
}

bool BasicSc2Bot::TryBuildBarracks() {
  // Build barracks if we have fewer than 2.
  if (CountUnits(UNIT_TYPEID::TERRAN_BARRACKS) < 2) {
    return TryBuildStructure(ABILITY_ID::BUILD_BARRACKS);
  }
  return false;
}

bool BasicSc2Bot::TryBuildStarport() {
  // Build Starport if we don't have one and if we have a Factory.
  if (CountUnits(UNIT_TYPEID::TERRAN_STARPORT) < 1 &&
      CountUnits(UNIT_TYPEID::TERRAN_FACTORY) > 0) {
    return TryBuildStructure(ABILITY_ID::BUILD_STARPORT,
                             UNIT_TYPEID::TERRAN_SCV);
  }
  return false;
}

bool BasicSc2Bot::TryBuildFactory() {
  // Build Factory if we have built the 20 Marines.
  if (CountUnits(UNIT_TYPEID::TERRAN_MARINE) >= 5) {
    if (CountUnits(UNIT_TYPEID::TERRAN_FACTORY) < 1) {
      return TryBuildStructure(ABILITY_ID::BUILD_FACTORY,
                               UNIT_TYPEID::TERRAN_SCV);
    }
  }
  return false;
}

// TROOP MANAGEMENT

void BasicSc2Bot::ManageAllTroops() {
  // // patrol command for Marines
  // Units marines = Observation()->GetUnits(Unit::Alliance::Self,
  //                                         IsUnit(UNIT_TYPEID::TERRAN_MARINE));

  // // PUT SCOUTING HERE

  // for (const auto &marine : marines) {
  //   if (marine->orders.empty()) { // If marine is idle
  //     Actions()->UnitCommand(marine, ABILITY_ID::GENERAL_PATROL,
  //                            GetBaseLocation());
  //   }
  // }

  // patrol command for Hellions
  Units hellions = Observation()->GetUnits(Unit::Alliance::Self,
                                           IsUnit(UNIT_TYPEID::TERRAN_HELLION));
  for (const auto &hellion : hellions) {
    if (hellion->orders.empty()) { // If hellion is idle
      Actions()->UnitCommand(hellion, ABILITY_ID::GENERAL_PATROL,
                             GetBaseLocation());
    }
  }

  // patrol command for Siege Tanks
  Units siegeTanks = Observation()->GetUnits(
      Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_SIEGETANK));
  for (const auto &tank : siegeTanks) {
    if (tank->orders.empty()) {
      Actions()->UnitCommand(tank, ABILITY_ID::GENERAL_PATROL,
                             GetBaseLocation());
    }
  }

  // patrol command for Medivacs
  Units medivacs = Observation()->GetUnits(Unit::Alliance::Self,
                                           IsUnit(UNIT_TYPEID::TERRAN_MEDIVAC));
  for (const auto &medivac : medivacs) {
    if (medivac->orders.empty()) { // If medivac is idle
      Actions()->UnitCommand(medivac, ABILITY_ID::GENERAL_PATROL,
                             GetBaseLocation());
    }
  }

  // patrol command for Vikings
  Units vikings = Observation()->GetUnits(
      Unit::Alliance::Self, IsUnit(UNIT_TYPEID::TERRAN_VIKINGFIGHTER));
  for (const auto &viking : vikings) {
    if (viking->orders.empty()) {
      Actions()->UnitCommand(viking, ABILITY_ID::GENERAL_PATROL,
                             GetBaseLocation());
    }
  }
}

// MODULAR UNIT TRAINING FUNCTIONS

void BasicSc2Bot::ManageBarracks() {
  // If we have fewer than 20 Marines, attempt to train Barracks.
  if (CountUnits(UNIT_TYPEID::TERRAN_MARINE) < 20) {
    Units barracks = Observation()->GetUnits(
        Unit::Alliance::Self,
        IsUnit(UNIT_TYPEID::TERRAN_BARRACKS)); // Get list of all barracks on
                                               // our team.
    for (const auto &barrack : barracks) {
      if (barrack->orders.empty()) {
        Actions()->UnitCommand(
            barrack,
            ABILITY_ID::TRAIN_MARINE); // Order barracks to train Marine if no
                                       // orders given yet.
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
    if (factory->add_on_tag == NullTag) {
      // If Factory has no add-on, order it to build a Tech Lab
      Actions()->UnitCommand(factory, ABILITY_ID::BUILD_TECHLAB);
    } else if (add_on &&
               add_on->unit_type == UNIT_TYPEID::TERRAN_FACTORYTECHLAB) {
      // If Factory has a Tech Lab, order it to produce Hellions and Siege
      // Tanks

      if (CountUnits(UNIT_TYPEID::TERRAN_SIEGETANK) > 5 &&
          CountUnits(UNIT_TYPEID::TERRAN_HELLION) < 5 &&
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
      // If the Starport has the Tech Lab add-on and it is idle, order VIKING
      // creation IFF we have fewer than 5 Vikings
      if (CountUnits(UNIT_TYPEID::TERRAN_VIKINGFIGHTER) < 5) {
        Actions()->UnitCommand(starport, ABILITY_ID::TRAIN_VIKINGFIGHTER);
      }
      // only make medivacs if there are 5 vikings
      if (CountUnits(UNIT_TYPEID::TERRAN_VIKINGFIGHTER) >= 5 &&
          CountUnits(UNIT_TYPEID::TERRAN_MEDIVAC) < 3 &&
          starport->orders.empty()) {
        // If the Starport is still idle, then we have maxed VIKING and Tanks.
        // Attempt to train Medivac (if not maxed.)
        Actions()->UnitCommand(starport, ABILITY_ID::TRAIN_MEDIVAC);
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
  const ObservationInterface *observation = Observation();
  const Unit *command_center =
      observation
          ->GetUnits(Unit::Alliance::Self,
                     IsUnit(UNIT_TYPEID::TERRAN_COMMANDCENTER))
          .front();
  return command_center->pos;
}
