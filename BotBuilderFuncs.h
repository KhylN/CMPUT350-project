#ifndef BOT_BUILDER_FUNCS
#define BOT_BUILDER_FUNCS

sc2::Point2D FindBuildLocation(sc2::Point2D base_location, sc2::ABILITY_ID ability_type);
bool TryBuildStructure(sc2::ABILITY_ID ability_type_for_structure);

bool TryBuildSupplyDepot();
bool TryMorphSupplyDepot();

bool TryBuildBarracks();
bool TryBuildStarport();
bool TryBuildFactory();
void TryBuildRefinery();

#endif
