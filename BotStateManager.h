#include "sc2api/sc2_api.h"
#include "sc2api/sc2_args.h"
#include "sc2api/sc2_unit_filters.h" // needed for IsUnit
#include "sc2lib/sc2_lib.h"
#include "sc2utils/sc2_arg_parser.h"
#include "sc2utils/sc2_manage_process.h"

#ifndef STATE_MANAGER
#define STATE_MANAGER

class StateManager {
    public:
        int step_count = 0;
        
        void InitializeEnemyLocations() {
            const sc2::GameInfo &game_info = SC2APIProtocol::Observation().GetGameInfo();
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

        // Gets current state (counts, flags) for current step
        void GetCurrentState() {
            const ObservationInterface *observation = SC2APIProtocol::Observation();

            num_marines = CountUnits(sc2::UNIT_TYPEID::TERRAN_MARINE);
            num_tanks = CountUnits(sc2::UNIT_TYPEID::TERRAN_SIEGETANK) + CountUnits(sc2::UNIT_TYPEID::TERRAN_SIEGETANKSIEGED);
            num_medivacs = CountUnits(sc2::UNIT_TYPEID::TERRAN_MEDIVAC);
            num_command_center = CountUnits(sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER);
            num_orbital_command = CountUnits(sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND);
            num_barracks = CountUnits(sc2::UNIT_TYPEID::TERRAN_BARRACKS);
            num_factory = CountUnits(sc2::UNIT_TYPEID::TERRAN_FACTORY);
            num_starport = CountUnits(sc2::UNIT_TYPEID::TERRAN_STARPORT);

            if (HasUpgrade(observation, sc2::UPGRADE_ID::STIMPACK) == true) {
                stim_done = true;
            }

            if (HasUpgrade(observation, sc2::UPGRADE_ID::COMBATSHIELD) == true) {
                shields_done = true;
            }
        }

        // Gets whether we have a certain upgrade.
        bool HasUpgrade(const sc2::ObservationInterface *observation, sc2::UpgradeID upgrade_id) {
            const auto &upgrades = observation->GetUpgrades();
            return std::find(upgrades.begin(), upgrades.end(), upgrade_id) != upgrades.end();
        }

        bool GetUpgradeStatus(sc2::UpgradeID upgrade_id) {
            switch (upgrade_id) {
                case(sc2::UPGRADE_ID::STIMPACK): {
                    return stim_done;
                }
                case(sc2::UPGRADE_ID::COMBATSHIELD): {
                    return shields_done;
                }

            }
        }

        int GetUnitCount(sc2::UNIT_TYPEID unit_type) {
            switch (unit_type)
            {
            case (sc2::UNIT_TYPEID::TERRAN_MARINE): {
                return num_marines;
            }
            case (sc2::UNIT_TYPEID::TERRAN_SIEGETANK): {
                return num_tanks;
            }
            case (sc2::UNIT_TYPEID::TERRAN_MEDIVAC): {
                return num_medivacs;
            }
            case (sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER): {
                return num_command_center;
            }
            case (sc2::UNIT_TYPEID::TERRAN_ORBITALCOMMAND): {
                return num_orbital_command;
            }
            case (sc2::UNIT_TYPEID::TERRAN_BARRACKS): {
                return num_barracks;
            }
            case (sc2::UNIT_TYPEID::TERRAN_FACTORY): {
                return num_factory;
            }
            case (sc2::UNIT_TYPEID::TERRAN_STARPORT): {
                return num_starport;
            }
            case (sc2::UNIT_TYPEID::TERRAN_ENGINEERINGBAY): {
                return num_enggbay;
            }
            
            default:
                break;
            }
            return -1; // Return -1 if failed to count.
        }

        // Get Base Location 
        sc2::Point2D GetBaseLocation() {
            if (base_location == sc2::Point2D()) {
                // If base location is not set, then do so.
                const ObservationInterface *observation = SC2APIProtocol::Observation();
                const Unit *command_center =
                    observation
                        ->GetUnits(Unit::Alliance::Self,
                                IsUnit(UNIT_TYPEID::TERRAN_COMMANDCENTER))
                        .front();
                base_location =
                    command_center->pos; // Set base location
            }
            // Return base location
            return base_location;
        }

        // Set Enemy Base Location (Dictated by Scout)
        sc2::Point2D SetEnemyBaseLocation(sc2::Point2D new_location) {
            enemy_base_location = new_location;
        }

        // Set Satellite Base Location (Dictated by Scout)
        sc2::Point2D SetSatelliteBaseLocation(sc2::Point2D new_location) {
            satellite_location = new_location;
        }

        // Get Enemy Base Location if it exists. Otherwise, return a blank Point2D.
        sc2::Point2D GetEnemyBaseLocation() {
            return base_location;
        }

        // Get Satellite Base Location if it exists. Otherwise, return a blank Point2D.
        sc2::Point2D GetSatelliteLocation() {
            return satellite_location;
        }

        bool SetSatelliteBuilt(bool new_value) {
            satellite_built = new_value;
        }

        bool GetSatelliteBuilt() {
            return satellite_built;
        }

        // Get Resource Locations
        const sc2::Unit *FindNearestMineralPatch(const sc2::Point2D &start) {
            Units units = SC2APIProtocol::Observation().GetUnits(Unit::Alliance::Neutral);
            float distance = std::numeric_limits<float>::max();
            const Unit *target = nullptr;
            for (const auto &u : units) {
                // Search for neutral mineral fields within a certain distance of the unit.
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

        const sc2::Unit *FindNearestVespene(const sc2::Point2D &start) {
            // Search for neutral Vespene geysers within a certain distance of the unit.
            Units units = SC2APIProtocol::Observation().GetUnits(Unit::Alliance::Neutral);
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
        
        bool CanAttack() {
            return (num_marines >= 40 && num_tanks >= 6 && num_medivacs >= 2);
        }

        size_t current_attack_wave_ = 1;
    private:
        // Helper funciton to determine how many of a cetain unit we have
        int CountUnits(sc2::UNIT_TYPEID unit_type) const {
            const ObservationInterface *observation = SC2APIProtocol::Observation();
            Units units = observation->GetUnits(Unit::Alliance::Self);
            int count = 0;
            for (const auto &unit : units) {
                if (unit->unit_type == unit_type) {
                ++count;
                }
            }
            return count;
        }

        // Base, Expansion Locations
        sc2::Point2D base_location = sc2::Point2D();
        sc2::Point2D satellite_location = sc2::Point2D();

        sc2::Point2D enemy_base_location; // Enemy base location when identified

        //sc2::Point2D main_base_mineral_patch = sc2::Point2D();
        bool satellite_built = false;

        // Troops
        int num_scvs{0}, num_marines{0}, num_tanks{0}, num_medivacs{0}, num_mules{0};

        // Buildings
        int num_command_center{0}, num_orbital_command{0}, num_barracks{0}, num_factory{0}, num_starport{0}, num_enggbay{0};}

        // Upgrade Flags
        bool stim_done = shields_done = false;

        const int THRESH = 30;
        std::vector<sc2::Point2D> potential_enemy_locations_;
}

#endif