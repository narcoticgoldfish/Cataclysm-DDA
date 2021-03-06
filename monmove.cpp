// Monster movement code; essentially, the AI

#include "monster.h"
#include "map.h"
#include "game.h"
#include "line.h"
#include "rng.h"
#include "pldata.h"
#include <stdlib.h>
#include "cursesdef.h"

//Used for e^(x) functions
#include <stdio.h>
#include <math.h>

#ifndef SGN
#define SGN(a) (((a)<0) ? -1 : 1)
#endif

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#define MONSTER_FOLLOW_DIST 8

void monster::receive_moves()
{
 moves += speed;
}

bool monster::wander()
{
 return (plans.empty());
}

bool monster::can_move_to(game *g, int x, int y)
{
    if (g->m.move_cost(x, y) == 0 &&
     (!has_flag(MF_DESTROYS) || !g->m.is_destructable(x, y)) &&
     ((!has_flag(MF_AQUATIC) && !has_flag(MF_SWIMS)) ||
      !g->m.has_flag(swimmable, x, y)))
        return false;
    if (has_flag(MF_DIGS) && !g->m.has_flag(diggable, x, y))
        return false;
    if (has_flag(MF_AQUATIC) && !g->m.has_flag(swimmable, x, y))
    return false;

    // various animal behaviours
    if (has_flag(MF_ANIMAL))
    {
        // don't enter sharp terrain unless tiny, or attacking
        if (g->m.has_flag(sharp, x, y) && !(attitude(&(g->u)) == MATT_ATTACK || type->size == MS_TINY))
            return false;

        // don't enter open pits ever unless tiny or can fly
        if (!(type->size == MS_TINY || has_flag(MF_FLIES)) && (g->m.ter(x, y) == t_pit || g->m.ter(x, y) == t_pit_spiked))
            return false;

        // don't enter lava ever
        if (g->m.ter(x, y) == t_lava)
            return false;

        // don't enter fire or electricity ever
        if (g->m.field_at(x, y).findField(fd_fire) || g->m.field_at(x, y).findField(fd_electricity))
            return false;
    }
    return true;
}

// Resets plans (list of squares to visit) and builds it as a straight line
// to the destination (x,y). t is used to choose which eligable line to use.
// Currently, this assumes we can see (x,y), so shouldn't be used in any other
// circumstance (or else the monster will "phase" through solid terrain!)
void monster::set_dest(int x, int y, int &t)
{
 plans.clear();
// TODO: This causes a segfault, once in a blue moon!  Whyyyyy.
 plans = line_to(posx, posy, x, y, t);
}

// Move towards (x,y) for f more turns--generally if we hear a sound there
// "Stupid" movement; "if (wandx < posx) posx--;" etc.
void monster::wander_to(int x, int y, int f)
{
 wandx = x;
 wandy = y;
 wandf = f;
 if (has_flag(MF_GOODHEARING))
  wandf *= 6;
}

void monster::plan(game *g)
{
 int sightrange = g->light_level();
 int closest = -1;
 int dist = 1000;
 int tc, stc;
 bool fleeing = false;
 if (friendly != 0) {	// Target monsters, not the player!
  for (int i = 0; i < g->z.size(); i++) {
   monster *tmp = &(g->z[i]);
   if (tmp->friendly == 0 && rl_dist(posx, posy, tmp->posx, tmp->posy) < dist &&
       g->m.sees(posx, posy, tmp->posx, tmp->posy, sightrange, tc)) {
    closest = i;
    dist = rl_dist(posx, posy, tmp->posx, tmp->posy);
    stc = tc;
   }
  }
  if (has_effect(ME_DOCILE))
   closest = -1;
  if (closest >= 0)
   set_dest(g->z[closest].posx, g->z[closest].posy, stc);
  else if (friendly > 0 && one_in(3))	// Grow restless with no targets
   friendly--;
  else if (friendly < 0 && g->sees_u(posx, posy, tc)) {
   if (rl_dist(posx, posy, g->u.posx, g->u.posy) > 2)
    set_dest(g->u.posx, g->u.posy, tc);
   else
    plans.clear();
  }
  return;
 }
 if (is_fleeing(g->u) && can_see() && g->sees_u(posx, posy, tc)) {
  fleeing = true;
  wandx = posx * 2 - g->u.posx;
  wandy = posy * 2 - g->u.posy;
  wandf = 40;
  dist = rl_dist(posx, posy, g->u.posx, g->u.posy);
 }
// If we can see, and we can see a character, start moving towards them
 if (!is_fleeing(g->u) && can_see() && g->sees_u(posx, posy, tc)) {
  dist = rl_dist(posx, posy, g->u.posx, g->u.posy);
  closest = -2;
  stc = tc;
 }
 for (int i = 0; i < g->active_npc.size(); i++) {
  npc *me = (g->active_npc[i]);
  int medist = rl_dist(posx, posy, me->posx, me->posy);
  if ((medist < dist || (!fleeing && is_fleeing(*me))) &&
      (can_see() &&
       g->m.sees(posx, posy, me->posx, me->posy, sightrange, tc))) {
   if (is_fleeing(*me)) {
    fleeing = true;
    wandx = posx * 2 - me->posx;
    wandy = posy * 2 - me->posy;
    wandf = 40;
    dist = medist;
   } else if (can_see() &&
              g->m.sees(posx, posy, me->posx, me->posy, sightrange, tc)) {
    dist = rl_dist(posx, posy, me->posx, me->posy);
    closest = i;
    stc = tc;
   }
  }
 }
 if (!fleeing) {
  fleeing = attitude() == MATT_FLEE;
  for (int i = 0; i < g->z.size(); i++) {
   monster *mon = &(g->z[i]);
   int mondist = rl_dist(posx, posy, mon->posx, mon->posy);
   if (mon->friendly != 0 && mondist < dist && can_see() &&
       g->m.sees(posx, posy, mon->posx, mon->posy, sightrange, tc)) {
    dist = mondist;
    if (fleeing) {
     wandx = posx * 2 - mon->posx;
     wandy = posy * 2 - mon->posy;
     wandf = 40;
    } else {
     closest = -3 - i;
     stc = tc;
    }
   }
  }
 }
 if (!fleeing) {
  if (closest == -2)
   set_dest(g->u.posx, g->u.posy, stc);
  else if (closest <= -3)
   set_dest(g->z[-3 - closest].posx, g->z[-3 - closest].posy, stc);
  else if (closest >= 0)
   set_dest(g->active_npc[closest]->posx, g->active_npc[closest]->posy, stc);
 }
}

// General movement.
// Currently, priority goes:
// 1) Special Attack
// 2) Sight-based tracking
// 3) Scent-based tracking
// 4) Sound-based tracking
void monster::move(game *g)
{
// We decrement wandf no matter what.  We'll save our wander_to plans until
// after we finish out set_dest plans, UNLESS they time out first.
 if (wandf > 0)
  wandf--;

// First, use the special attack, if we can!
 if (sp_timeout > 0)
  sp_timeout--;
 if (sp_timeout == 0 && (friendly == 0 || has_flag(MF_FRIENDLY_SPECIAL))) {
  mattack ma;
  (ma.*type->sp_attack)(g, this);
 }
 if (moves < 0)
  return;
 if (has_flag(MF_IMMOBILE)) {
  moves = 0;
  return;
 }
 if (has_effect(ME_STUNNED)) {
  stumble(g, false);
  moves = 0;
  return;
 }
 if (has_effect(ME_DOWNED)) {
  moves = 0;
  return;
 }
 if (has_effect(ME_BOULDERING)){
  moves -= 20;
  if (moves < 0)
   moves = 0;
  return;
 }
 if (friendly != 0) {
  if (friendly > 0)
   friendly--;
  friendly_move(g);
  return;
 }

 bool moved = false;
 point next;
 int mondex = (plans.size() > 0 ? g->mon_at(plans[0].x, plans[0].y) : -1);

 monster_attitude current_attitude = attitude();
 if (friendly == 0)
  current_attitude = attitude(&(g->u));
// If our plans end in a player, set our attitude to consider that player
 if (plans.size() > 0) {
  if (plans.back().x == g->u.posx && plans.back().y == g->u.posy)
   current_attitude = attitude(&(g->u));
  else {
   for (int i = 0; i < g->active_npc.size(); i++) {
    if (plans.back().x == g->active_npc[i]->posx &&
        plans.back().y == g->active_npc[i]->posy)
     current_attitude = attitude((g->active_npc[i]));
   }
  }
 }

 if (current_attitude == MATT_IGNORE ||
     (current_attitude == MATT_FOLLOW && plans.size() <= MONSTER_FOLLOW_DIST)) {
  moves -= 100;
  stumble(g, false);
  return;
 }

 if (plans.size() > 0 && !is_fleeing(g->u) &&
     (mondex == -1 || g->z[mondex].friendly != 0 || has_flag(MF_ATTACKMON)) &&
     (can_move_to(g, plans[0].x, plans[0].y) ||
      (plans[0].x == g->u.posx && plans[0].y == g->u.posy) ||
     (g->m.has_flag(bashable, plans[0].x, plans[0].y) && has_flag(MF_BASHES)))){
  // CONCRETE PLANS - Most likely based on sight
  next = plans[0];
  moved = true;
 } else if (has_flag(MF_SMELLS)) {
// No sight... or our plans are invalid (e.g. moving through a transparent, but
//  solid, square of terrain).  Fall back to smell if we have it.
  point tmp = scent_move(g);
  if (tmp.x != -1) {
   next = tmp;
   moved = true;
  }
 }
 if (wandf > 0 && !moved) { // No LOS, no scent, so as a fall-back follow sound
  point tmp = sound_move(g);
  if (tmp.x != posx || tmp.y != posy) {
   next = tmp;
   moved = true;
  }
 }

// Finished logic section.  By this point, we should have chosen a square to
//  move to (moved = true).
 if (moved) {	// Actual effects of moving to the square we've chosen
  mondex = g->mon_at(next.x, next.y);
  int npcdex = g->npc_at(next.x, next.y);
  if (next.x == g->u.posx && next.y == g->u.posy && type->melee_dice > 0)
   hit_player(g, g->u);
  else if (mondex != -1 && g->z[mondex].type->species == species_hallu) {
   g->kill_mon(mondex);
   moves -= 100;
  } else if (mondex != -1 && type->melee_dice > 0 && this != &(g->z[mondex]) &&
             (g->z[mondex].friendly != 0 || has_flag(MF_ATTACKMON)))
   hit_monster(g, mondex);
  else if (npcdex != -1 && type->melee_dice > 0)
   hit_player(g, *g->active_npc[npcdex]);
  else if ((!can_move_to(g, next.x, next.y) || one_in(3)) &&
             g->m.has_flag(bashable, next.x, next.y) && has_flag(MF_BASHES)) {
   std::string bashsound = "NOBASH"; // If we hear "NOBASH" it's time to debug!
   int bashskill = int(type->melee_dice * type->melee_sides);
   g->m.bash(next.x, next.y, bashskill, bashsound);
   g->sound(next.x, next.y, 18, bashsound);
   moves -= 100;
  } else if (g->m.move_cost(next.x, next.y) == 0 && has_flag(MF_DESTROYS)) {
   g->m.destroy(g, next.x, next.y, true);
   moves -= 250;
  } else if (can_move_to(g, next.x, next.y) && g->is_empty(next.x, next.y))
   move_to(g, next.x, next.y);
  else
   moves -= 100;
 } else
  moves -= 100;

// If we're close to our target, we get focused and don't stumble
 if ((has_flag(MF_STUMBLES) && (plans.size() > 3 || plans.size() == 0)) ||
     !moved)
  stumble(g, moved);
}

// footsteps will determine how loud a monster's normal movement is
// and create a sound in the monsters location when they move
void monster::footsteps(game *g, int x, int y)
{
 if (made_footstep)
  return;
 if (has_flag(MF_FLIES))
  return; // Flying monsters don't have footsteps!
 made_footstep = true;
 int volume = 6; // same as player's footsteps
 if (has_flag(MF_DIGS))
  volume = 10;
 switch (type->size) {
  case MS_TINY:
   return; // No sound for the tinies
  case MS_SMALL:
   volume /= 3;
   break;
  case MS_MEDIUM:
   break;
  case MS_LARGE:
   volume *= 1.5;
   break;
  case MS_HUGE:
   volume *= 2;
   break;
  default: break;
 }
 int dist = rl_dist(x, y, g->u.posx, g->u.posy);
 g->add_footstep(x, y, volume, dist, this);
 return;
}

void monster::friendly_move(game *g)
{
 point next;
 bool moved = false;
 if (plans.size() > 0 && (plans[0].x != g->u.posx || plans[0].y != g->u.posy) &&
     (can_move_to(g, plans[0].x, plans[0].y) ||
     (g->m.has_flag(bashable, plans[0].x, plans[0].y) && has_flag(MF_BASHES)))){
  next = plans[0];
  plans.erase(plans.begin());
  moved = true;
 } else {
  moves -= 100;
  stumble(g, moved);
 }
 if (moved) {
  int mondex = g->mon_at(next.x, next.y);
  int npcdex = g->npc_at(next.x, next.y);
  if (mondex != -1 && g->z[mondex].friendly == 0 && type->melee_dice > 0)
   hit_monster(g, mondex);
  else if (npcdex != -1 && type->melee_dice > 0)
   hit_player(g, *g->active_npc[g->npc_at(next.x, next.y)]);
  else if (mondex == -1 && npcdex == -1 && can_move_to(g, next.x, next.y))
   move_to(g, next.x, next.y);
  else if ((!can_move_to(g, next.x, next.y) || one_in(3)) &&
           g->m.has_flag(bashable, next.x, next.y) && has_flag(MF_BASHES)) {
   std::string bashsound = "NOBASH"; // If we hear "NOBASH" it's time to debug!
   int bashskill = int(type->melee_dice * type->melee_sides);
   g->m.bash(next.x, next.y, bashskill, bashsound);
   g->sound(next.x, next.y, 18, bashsound);
   moves -= 100;
  } else if (g->m.move_cost(next.x, next.y) == 0 && has_flag(MF_DESTROYS)) {
   g->m.destroy(g, next.x, next.y, true);
   moves -= 250;
  }
 }
}

point monster::scent_move(game *g)
{
 plans.clear();
 std::vector<point> smoves;

 int maxsmell = 2; // Squares with smell 0 are not eligable targets
 if (has_flag(MF_KEENNOSE))
 {
     maxsmell = 1;
 }
 int minsmell = 9999;
 point pbuff, next(-1, -1);
 unsigned int smell;
 for (int x = -1; x <= 1; x++) {
  for (int y = -1; y <= 1; y++) {
   smell = g->scent(posx + x, posy + y);
   int mon = g->mon_at(posx + x, posy + y);
   if ((mon == -1 || g->z[mon].friendly != 0 || has_flag(MF_ATTACKMON)) &&
       (can_move_to(g, posx + x, posy + y) ||
        (posx + x == g->u.posx && posx + y == g->u.posy) ||
        (g->m.has_flag(bashable, posx + x, posy + y) && has_flag(MF_BASHES)))) {
    if ((!is_fleeing(g->u) && smell > maxsmell) ||
        ( is_fleeing(g->u) && smell < minsmell)   ) {
     smoves.clear();
     pbuff.x = posx + x;
     pbuff.y = posy + y;
     smoves.push_back(pbuff);
     maxsmell = smell;
     minsmell = smell;
    } else if ((!is_fleeing(g->u) && smell == maxsmell) ||
               ( is_fleeing(g->u) && smell == minsmell)   ) {
     pbuff.x = posx + x;
     pbuff.y = posy + y;
     smoves.push_back(pbuff);
    }
   }
  }
 }
 if (smoves.size() > 0) {
  int nextsq = rng(0, smoves.size() - 1);
  next = smoves[nextsq];
 }
 return next;
}

point monster::sound_move(game *g)
{
 plans.clear();
 point next;
 bool xbest = true;
 if (abs(wandy - posy) > abs(wandx - posx))// which is more important
  xbest = false;
 next.x = posx;
 next.y = posy;
 int x = posx, x2 = posx - 1, x3 = posx + 1;
 int y = posy, y2 = posy - 1, y3 = posy + 1;
 if (wandx < posx) { x--; x2++;          }
 if (wandx > posx) { x++; x2++; x3 -= 2; }
 if (wandy < posy) { y--; y2++;          }
 if (wandy > posy) { y++; y2++; y3 -= 2; }
 if (xbest) {
  if (can_move_to(g, x, y) || (x == g->u.posx && y == g->u.posy) ||
      (has_flag(MF_BASHES) && g->m.has_flag(bashable, x, y))) {
   next.x = x;
   next.y = y;
  } else if (can_move_to(g, x, y2) || (x == g->u.posx && y == g->u.posy) ||
             (has_flag(MF_BASHES) && g->m.has_flag(bashable, x, y2))) {
   next.x = x;
   next.y = y2;
  } else if (can_move_to(g, x2, y) || (x == g->u.posx && y == g->u.posy) ||
             (has_flag(MF_BASHES) && g->m.has_flag(bashable, x2, y))) {
   next.x = x2;
   next.y = y;
  } else if (can_move_to(g, x, y3) || (x == g->u.posx && y == g->u.posy) ||
             (has_flag(MF_BASHES) && g->m.has_flag(bashable, x, y3))) {
   next.x = x;
   next.y = y3;
  } else if (can_move_to(g, x3, y) || (x == g->u.posx && y == g->u.posy) ||
             (has_flag(MF_BASHES) && g->m.has_flag(bashable, x3, y))) {
   next.x = x3;
   next.y = y;
  }
 } else {
  if (can_move_to(g, x, y) || (x == g->u.posx && y == g->u.posy) ||
      (has_flag(MF_BASHES) && g->m.has_flag(bashable, x, y))) {
   next.x = x;
   next.y = y;
  } else if (can_move_to(g, x2, y) || (x == g->u.posx && y == g->u.posy) ||
             (has_flag(MF_BASHES) && g->m.has_flag(bashable, x2, y))) {
   next.x = x2;
   next.y = y;
  } else if (can_move_to(g, x, y2) || (x == g->u.posx && y == g->u.posy) ||
             (has_flag(MF_BASHES) && g->m.has_flag(bashable, x, y2))) {
   next.x = x;
   next.y = y2;
  } else if (can_move_to(g, x3, y) || (x == g->u.posx && y == g->u.posy) ||
             (has_flag(MF_BASHES) && g->m.has_flag(bashable, x3, y))) {
   next.x = x3;
   next.y = y;
  } else if (can_move_to(g, x, y3) || (x == g->u.posx && y == g->u.posy) ||
             (has_flag(MF_BASHES) && g->m.has_flag(bashable, x, y3))) {
   next.x = x;
   next.y = y3;
  }
 }
 return next;
}

void monster::hit_player(game *g, player &p, bool can_grab)
{
    moves -= 100;

    if (type->melee_dice == 0) // We don't attack, so just return
    {
        return;
    }
    add_effect(ME_HIT_BY_PLAYER, 3); // Make us a valid target for a few turns
    if (has_flag(MF_HIT_AND_RUN))
    {
        add_effect(ME_RUN, 4);
    }
    bool is_npc = p.is_npc();
    bool u_see = (!is_npc || g->u_see(p.posx, p.posy));
    std::string you  = (is_npc ? p.name : "you");
    std::string You  = (is_npc ? p.name : "You");
    std::string your = (is_npc ? p.name + "'s" : "your");
    std::string Your = (is_npc ? p.name + "'s" : "Your");
    body_part bphit;
    int side = rng(0, 1);
    int dam = hit(g, p, bphit), cut = type->melee_cut, stab = 0;
    technique_id tech = p.pick_defensive_technique(g, this, NULL);
    p.perform_defensive_technique(tech, g, this, NULL, bphit, side, dam, cut, stab);

    //110*e^(-.3*[melee skill of monster]) = % chance to miss. *100 to track .01%'s
    //Returns ~80% at 1, drops quickly to 33% at 4, then slowly to 5% at 10 and 1% at 16
    if (rng(0, 10000) < 11000 * exp(-.3 * type->melee_skill))
    {
        g->add_msg("The %s misses.", name().c_str());
    }
    else
    {
        //Reduce player's ability to dodge by monster's ability to hit
        int dodge_ii = p.dodge(g) - rng(0, type->melee_skill);
        if (dodge_ii < 0)
        {
            dodge_ii = 0;
        }

        // 100/(1+99*e^(-.6*[dodge() return modified by monster's skill])) = % chance to dodge
        // *100 to track .01%'s
        // 1% minimum, scales slowly to 16% at 5, then rapidly to 80% at 10,
        // then returns less with each additional point, reaching 99% at 16
        if (rng(0, 10000) < 10000/(1 + 99 * exp(-.6 * dodge_ii)))
        {
            g->add_msg("%s dodge the %s.", You.c_str(), name().c_str());
            p.practice(g->turn, "dodge", type->melee_skill * 2); //Better monster = more skill gained
        }

        //Successful hit with damage
        else if (dam > 0)
        {
            p.practice(g->turn, "dodge", type->melee_skill);
            if (u_see && tech != TEC_BLOCK)
            {
                g->add_msg("The %s hits %s %s.", name().c_str(), your.c_str(),
                           body_part_name(bphit, side).c_str());
            }

            // Attempt defensive moves
            if (!is_npc)
            {
                if (g->u.activity.type == ACT_RELOAD)
                {
                    g->add_msg("You stop reloading.");
                }
                else if (g->u.activity.type == ACT_READ)
                {
                    g->add_msg("You stop reading.");
                }
                else if (g->u.activity.type == ACT_CRAFT || g->u.activity.type == ACT_LONGCRAFT)
                {
                    g->add_msg("You stop crafting.");
                    g->u.activity.type = ACT_NULL;
                }
            }
            if (p.has_active_bionic("bio_ods"))
            {
                if (u_see)
                {
                    g->add_msg("%s offensive defense system shocks it!", Your.c_str());
                }
                if (hurt(rng(10, 40)))
                    die(g);
            }
            if (p.encumb(bphit) == 0 &&(p.has_trait(PF_SPINES) || p.has_trait(PF_QUILLS)))
            {
                int spine = rng(1, (p.has_trait(PF_QUILLS) ? 20 : 8));
                g->add_msg("%s %s puncture it!", Your.c_str(),
                           (g->u.has_trait(PF_QUILLS) ? "quills" : "spines"));
                if (hurt(spine))
                    die(g);
            }

            if (dam + cut <= 0)
            {
                return; // Defensive technique canceled damage.
            }

            //Hurt the player
            dam = p.hit(g, bphit, side, dam, cut);

            //Monster effects
            if (dam > 0 && has_flag(MF_VENOM))
            {
                if (!is_npc)
                {
                    g->add_msg("You're poisoned!");
                }
                p.add_disease("poison", 30);
            }
            else if (dam > 0 && has_flag(MF_BADVENOM))
            {
                if (!is_npc)
                {
                    g->add_msg("You feel poison flood your body, wracking you with pain...");
                }
                p.add_disease("badpoison", 40);
            }
            if (has_flag(MF_BLEED) && dam > 6 && cut > 0)
            {
                if (!is_npc)
                {
                    g->add_msg("You're Bleeding!");
                }
                p.add_disease("bleed", 60);
            }

            //Same as monster's chance to not miss
            if (can_grab && has_flag(MF_GRABS) && (rng(0, 10000) > 11000 * exp(-.3 * type->melee_skill)))
            {
                if (!is_npc)
                {
                    g->add_msg("The %s grabs you!", name().c_str());
                }
                if (p.weapon.has_technique(TEC_BREAK, &p) &&
                    dice(p.dex_cur + p.skillLevel("melee"), 12) > dice(type->melee_dice, 10))
                {
                    if (!is_npc)
                    {
                        g->add_msg("You break the grab!");
                    }
                }
                else
                    hit_player(g, p, false); //We grabed, so hit them again
            }

            //Counter-attack?
            if (tech == TEC_COUNTER && !is_npc)
            {
                g->add_msg("Counter-attack!");
                hurt( p.hit_mon(g, this) );
            }
        }
    }

    // if dam > 0
    if (is_npc)
    {
        if (p.hp_cur[hp_head] <= 0 || p.hp_cur[hp_torso] <= 0)
        {
            npc* tmp = dynamic_cast<npc*>(&p);
            tmp->die(g);
            int index = g->npc_at(p.posx, p.posy);
            if (index != -1 && index < g->active_npc.size())
            {
                g->active_npc.erase(g->active_npc.begin() + index);
            }
            plans.clear();
        }
    }

    // Adjust anger/morale of same-species monsters, if appropriate
    int anger_adjust = 0, morale_adjust = 0;
    for (int i = 0; i < type->anger.size(); i++)
    {
        if (type->anger[i] == MTRIG_FRIEND_ATTACKED)
        {
            anger_adjust += 15;
        }
    }
    for (int i = 0; i < type->placate.size(); i++)
    {
        if (type->placate[i] == MTRIG_FRIEND_ATTACKED)
        {
            anger_adjust -= 15;
        }
    }
    for (int i = 0; i < type->fear.size(); i++)
    {
        if (type->fear[i] == MTRIG_FRIEND_ATTACKED)
        {
            morale_adjust -= 15;
        }
    }
    if (anger_adjust != 0 && morale_adjust != 0)
    {
        for (int i = 0; i < g->z.size(); i++)
        {
            g->z[i].morale += morale_adjust;
            g->z[i].anger += anger_adjust;
        }
    }
}

int monster::calc_movecost(game *g, int x1, int y1, int x2, int y2)
{
    int movecost = 0;
    int diag_mult = (trigdist && x1 != x2 && y1 != y2) ? 1.41 : 1;

    // Digging and flying monsters ignore terrain cost
    if (has_flag(MF_DIGS) || has_flag(MF_FLIES)) {
        movecost = 100 * diag_mult;
    // Swimming monsters move super fast in water
    } else if (has_flag(MF_SWIMS)) {
        if (g->m.has_flag(swimmable, x1, y1))
            movecost += 25;
        else
            movecost += 50 * g->m.move_cost(x1, y1);
        if (g->m.has_flag(swimmable, x2, y2))
            movecost += 25;
        else
            movecost += 50 * g->m.move_cost(x2, y2);
        movecost *= diag_mult;
    // All others use the same calculation as the player
    } else {
        movecost = (g->m.combined_movecost(x1, y1, x2, y2));
    }

    return movecost;
}

void monster::move_to(game *g, int x, int y)
{
 int mondex = g->mon_at(x, y);
 if (mondex == -1) { //...assuming there's no monster there
  if (has_effect(ME_BEARTRAP)) {
   moves = 0;
   return;
  }
  
  if (plans.size() > 0)
   plans.erase(plans.begin());

  moves -= calc_movecost(g, posx, posy, x, y);

  if (has_flag(MF_SLUDGETRAIL)){
   g->m.add_field(g, posx, posy, fd_sludge, 3);
   g->m.add_field(g, posx+1, posy, fd_sludge, 2);
   g->m.add_field(g, posx, posy+1, fd_sludge, 2);
   g->m.add_field(g, posx-1, posy, fd_sludge, 2);
   g->m.add_field(g, posx, posy-1, fd_sludge, 2);
  }

  posx = x;
  posy = y;
  footsteps(g, x, y);
  if (type->size != MS_TINY && g->m.has_flag(sharp, posx, posy) && !one_in(4))
     hurt(rng(2, 3));
  if (type->size != MS_TINY && g->m.has_flag(rough, posx, posy) && one_in(6))
     hurt(rng(1, 2));
  if (!has_flag(MF_DIGS) && !has_flag(MF_FLIES) &&
      g->m.tr_at(posx, posy) != tr_null) { // Monster stepped on a trap!
   trap* tr = g->traps[g->m.tr_at(posx, posy)];
   if (dice(3, type->sk_dodge + 1) < dice(3, tr->avoidance)) {
    trapfuncm f;
    (f.*(tr->actm))(g, this, posx, posy);
   }
  }
// Diggers turn the dirt into dirtmound
  if (has_flag(MF_DIGS)){
   g->m.ter_set(posx, posy, t_dirtmound);
  }
// Acid trail monsters leave... a trail of acid
  if (has_flag(MF_ACIDTRAIL)){
   g->m.add_field(g, posx, posy, fd_acid, 1);
  }
  if (has_flag(MF_ACIDTRAIL)){
   g->m.add_field(g, posx, posy, fd_acid, 1);
  }
  if (has_flag(MF_ACIDTRAIL)){
   g->m.add_field(g, posx, posy, fd_acid, 1);
  }

 } else if (has_flag(MF_ATTACKMON) || g->z[mondex].friendly != 0)
// If there IS a monster there, and we fight monsters, fight it!
  hit_monster(g, mondex);
}

/* Random walking even when we've moved
 * To simulate zombie stumbling and ineffective movement
 * Note that this is sub-optimal; stumbling may INCREASE a zombie's speed.
 * Most of the time (out in the open) this effect is insignificant compared to
 * the negative effects, but in a hallway it's perfectly even
 */
void monster::stumble(game *g, bool moved)
{
 // don't stumble every turn. every 3rd turn, or 8th when walking.
 if((moved && !one_in(8)) || !one_in(3))
 {
     return;
 }

 std::vector <point> valid_stumbles;
 for (int i = -1; i <= 1; i++) {
  for (int j = -1; j <= 1; j++) {
   if (can_move_to(g, posx + i, posy + j) &&
       (g->u.posx != posx + i || g->u.posy != posy + j) &&
       (g->mon_at(posx + i, posy + j) == -1 || (i == 0 && j == 0))) {
    point tmp(posx + i, posy + j);
    valid_stumbles.push_back(tmp);
   }
  }
 }
 if (valid_stumbles.size() == 0) //nowhere to stumble?
 {
     return;
 }

 int choice = rng(0, valid_stumbles.size() - 1);
 int cx = valid_stumbles[choice].x;
 int cy = valid_stumbles[choice].y;

 moves -= calc_movecost(g, posx, posy, cx, cy);
 posx = cx;
 posy = cy;

 // Here we have to fix our plans[] list,
 // acquiring a new path to the previous target.
 // target == either end of current plan, or the player.
 int tc;
 if (plans.size() > 0) {
  if (g->m.sees(posx, posy, plans.back().x, plans.back().y, -1, tc))
   set_dest(plans.back().x, plans.back().y, tc);
  else if (g->sees_u(posx, posy, tc))
   set_dest(g->u.posx, g->u.posy, tc);
  else //durr, i'm suddenly calm. what was i doing?
   plans.clear();
 }
}

void monster::knock_back_from(game *g, int x, int y)
{
 if (x == posx && y == posy)
  return; // No effect
 point to(posx, posy);
 if (x < posx)
  to.x++;
 if (x > posx)
  to.x--;
 if (y < posy)
  to.y++;
 if (y > posy)
  to.y--;

 bool u_see = g->u_see(to.x, to.y);

// First, see if we hit another monster
 int mondex = g->mon_at(to.x, to.y);
 if (mondex != -1) {
  monster *z = &(g->z[mondex]);
  hurt(z->type->size);
  add_effect(ME_STUNNED, 1);
  if (type->size > 1 + z->type->size) {
   z->knock_back_from(g, posx, posy); // Chain reaction!
   z->hurt(type->size);
   z->add_effect(ME_STUNNED, 1);
  } else if (type->size > z->type->size) {
   z->hurt(type->size);
   z->add_effect(ME_STUNNED, 1);
  }

  if (u_see)
   g->add_msg("The %s bounces off a %s!", name().c_str(), z->name().c_str());

  return;
 }

 int npcdex = g->npc_at(to.x, to.y);
 if (npcdex != -1) {
  npc *p = g->active_npc[npcdex];
  hurt(3);
  add_effect(ME_STUNNED, 1);
  p->hit(g, bp_torso, 0, type->size, 0);
  if (u_see)
   g->add_msg("The %s bounces off %s!", name().c_str(), p->name.c_str());

  return;
 }

// If we're still in the function at this point, we're actually moving a tile!
 if (g->m.move_cost(to.x, to.y) == 0) { // Wait, it's a wall (or water)

  if (g->m.has_flag(liquid, to.x, to.y)) {
   if (!has_flag(MF_SWIMS) && !has_flag(MF_AQUATIC)) {
    hurt(9999);
    if (u_see)
     g->add_msg("The %s drowns!", name().c_str());
   }

  } else if (has_flag(MF_AQUATIC)) { // We swim but we're NOT in water
   hurt(9999);
   if (u_see)
    g->add_msg("The %s flops around and dies!", name().c_str());

  } else { // It's some kind of wall.
   hurt(type->size);
   add_effect(ME_STUNNED, 2);
   if (u_see)
    g->add_msg("The %s bounces off a %s.", name().c_str(),
                                           g->m.tername(to.x, to.y).c_str());
  }

 } else { // It's no wall
  posx = to.x;
  posy = to.y;
 }
}


/* will_reach() is used for determining whether we'll get to stairs (and
 * potentially other locations of interest).  It is generally permissive.
 * TODO: Pathfinding;
         Make sure that non-smashing monsters won't "teleport" through windows
         Injure monsters if they're gonna be walking through pits or whatevs
 */
bool monster::will_reach(game *g, int x, int y)
{
 monster_attitude att = attitude(&(g->u));
 if (att != MATT_FOLLOW && att != MATT_ATTACK && att != MATT_FRIEND)
  return false;

 if (has_flag(MF_DIGS))
  return false;

 if (has_flag(MF_IMMOBILE) && (posx != x || posy != y))
  return false;

 std::vector<point> path = g->m.route(posx, posy, x, y, has_flag(MF_BASHES));
 if (path.size() == 0)
   return false;

 if (has_flag(MF_SMELLS) && g->scent(posx, posy) > 0 &&
     g->scent(x, y) > g->scent(posx, posy))
  return true;

 if (can_hear() && wandf > 0 && rl_dist(wandx, wandy, x, y) <= 2 &&
     rl_dist(posx, posy, wandx, wandy) <= wandf)
  return true;

 int t;
 if (can_see() && g->m.sees(posx, posy, x, y, g->light_level(), t))
  return true;

 return false;
}

int monster::turns_to_reach(game *g, int x, int y)
{
 std::vector<point> path = g->m.route(posx, posy, x, y, has_flag(MF_BASHES));
 if (path.size() == 0)
  return 999;

 double turns = 0.;
 for (int i = 0; i < path.size(); i++) {
  if (g->m.move_cost(path[i].x, path[i].y) == 0) // We have to bash through
   turns += 5;
  else if (i == 0)
   turns += double(calc_movecost(g, posx, posy, path[i].x, path[i].y)) / speed;
  else
   turns += double(calc_movecost(g, path[i-1].x, path[i-1].y, path[i].x, path[i].y)) / speed;
 }
 return int(turns + .9); // Round up
}
