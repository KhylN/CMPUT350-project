#ifndef BOT_ATTACK_FUNCS
#define BOT_ATTACK_FUNCS

void LaunchAttack();
bool IsArmyIdle();
void SendArmyTo(const sc2::Point2D &target);
bool HasSupportableGroundUnits();

bool barrack_tech_lab = false;
bool stim_done = false;
bool shields_done = false;
bool is_attacking = false;

#endif
