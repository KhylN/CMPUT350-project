#ifndef BOT_ATTACK_FUNCS
#define BOT_ATTACK_FUNCS

sc2::Point2D FindBuildLocation(sc2::Point2D base_location, sc2::ABILITY_ID ability_type);
bool TryBuildStructure(sc2::ABILITY_ID ability_type_for_structure, sc2::UNIT_TYPEID unit_type = sc2::UNIT_TYPEID::TERRAN_SCV);

bool TryBuildSupplyDepot();
bool TryMorphSupplyDepot();

bool TryBuildBarracks();
bool TryBuildStarport();
bool TryBuildFactory();

#endif
