#ifndef BOT_GAME_STATE_FUNCS
#define BOT_GAME_STATE_FUNCS

bool HasUpgrade(const sc2::ObservationInterface *observation,
                sc2::UpgradeID upgrade_id);

int step = 0;
const int THRESH = 30;
void InitializeSatelliteLocation();
void InitializeEnemyLocations();
int CountUnits(sc2::UNIT_TYPEID unit_type);
sc2::Point2D GetBaseLocation();
const sc2::Unit *FindNearestMineralPatch(const sc2::Point2D &start);
const sc2::Unit *FindNearestVespene(const sc2::Point2D &start);
int MilitaryStrength();
std::vector<sc2::Point2D> potential_enemy_locations_;
size_t current_attack_wave_ = 1;

#endif
