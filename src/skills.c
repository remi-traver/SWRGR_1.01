/***************************************************************************
*                           STAR WARS REALITY 1.0                          *
*--------------------------------------------------------------------------*
* Star Wars Reality Code Additions and changes from the Smaug Code         *
* copyright (c) 1997 by Sean Cooper                                        *
* -------------------------------------------------------------------------*
* Starwars and Starwars Names copyright(c) Lucas Film Ltd.                 *
*--------------------------------------------------------------------------*
* SMAUG 1.0 (C) 1994, 1995, 1996 by Derek Snider                           *
* SMAUG code team: Thoric, Altrag, Blodkai, Narn, Haus,                    *
* Scryn, Rennard, Swordbearer, Gorog, Grishnakh and Tricops                *
* ------------------------------------------------------------------------ *
* Merc 2.1 Diku Mud improvments copyright (C) 1992, 1993 by Michael        *
* Chastain, Michael Quan, and Mitchell Tse.                                *
* Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,          *
* Michael Seifert, Hans Henrik St{rfeldt, Tom Madsen, and Katja Nyboe.     *
* ------------------------------------------------------------------------ *
*			     Player skills module			   *
****************************************************************************/

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "mud.h"

CHAR_DATA* get_char_room_mp(CHAR_DATA* ch, const char* argument); /*added MG*/

const char *const spell_flag[] = { "water", "earth", "air", "astral", "area", "distant", "reverse",
   "save_half_dam", "save_negates", "accumulative", "recastable", "noscribe",
   "nobrew", "group", "object", "character", "secretskill", "pksensitive"
};

const char *const spell_saves[] = { "none", "poison_death", "wands", "para_petri", "breath", "spell_staff" };

const char *const spell_damage[] = { "none", "fire", "cold", "electricity", "energy", "acid", "poison", "drain" };

const char *const spell_action[] = { "none", "create", "destroy", "resist", "suscept", "divinate", "obscure", "change" };

const char *const spell_power[] = { "none", "minor", "greater", "major" };

const char *const spell_class[] = { "none", "lunar", "solar", "travel", "summon", "life", "death", "illusion" };

const char *const target_type[] = { "ignore", "offensive", "defensive", "self", "objinv" };

void get_obj_palm args((CHAR_DATA * ch, OBJ_DATA * obj, OBJ_DATA * container));
void show_char_to_char( CHAR_DATA * list, CHAR_DATA * ch );
bool validate_spec_fun( const char *name );
int ris_save( CHAR_DATA * ch, int schance, int ris );
bool check_illegal_psteal( CHAR_DATA * ch, CHAR_DATA * victim );
void failed_casting( struct skill_type *skill, CHAR_DATA * ch, CHAR_DATA * victim, OBJ_DATA * obj );
int xp_compute( CHAR_DATA * gch, CHAR_DATA * victim );
int get_skill_percent( CHAR_DATA * ch, int sn);
bool knows_skill(CHAR_DATA * ch, int sn);
int check_reflexes(CHAR_DATA* ch);

/*
 * Dummy function
 */
void skill_notfound( CHAR_DATA * ch, const char *argument )
{
   send_to_char( "Huh?\r\n", ch );
   return;
}

int get_ssave( const char *name )
{
  size_t x;

  for( x = 0; x < sizeof( spell_saves ) / sizeof( spell_saves[0] ); x++ )
    if( !str_cmp( name, spell_saves[x] ) )
      return x;
  return -1;
}

int get_starget( const char *name )
{
  size_t x;

  for( x = 0; x < sizeof( target_type ) / sizeof( target_type[0] ); x++ )
    if( !str_cmp( name, target_type[x] ) )
      return x;
  return -1;
}

int get_sflag( const char *name )
{
  size_t x;

  for( x = 0; x < sizeof( spell_flag ) / sizeof( spell_flag[0] ); x++ )
    if( !str_cmp( name, spell_flag[x] ) )
      return x;
  return -1;
}

int get_sdamage( const char *name )
{
  size_t x;

  for( x = 0; x < sizeof( spell_damage ) / sizeof( spell_damage[0] ); x++ )
    if( !str_cmp( name, spell_damage[x] ) )
      return x;
  return -1;
}

int get_saction( const char *name )
{
  size_t x;

  for( x = 0; x < sizeof( spell_action ) / sizeof( spell_action[0] ); x++ )
    if( !str_cmp( name, spell_action[x] ) )
      return x;
  return -1;
}

int get_spower( const char *name )
{
  size_t x;

  for( x = 0; x < sizeof( spell_power ) / sizeof( spell_power[0] ); x++ )
    if( !str_cmp( name, spell_power[x] ) )
      return x;
  return -1;
}

int get_sclass( const char *name )
{
  size_t x;

  for( x = 0; x < sizeof( spell_class ) / sizeof( spell_class[0] ); x++ )
    if( !str_cmp( name, spell_class[x] ) )
      return x;
  return -1;
}

bool is_legal_kill( CHAR_DATA * ch, CHAR_DATA * vch )
{
   if( IS_NPC( ch ) || IS_NPC( vch ) )
      return TRUE;
   if( is_safe( ch, vch ) )
      return FALSE;
   return TRUE;
}

extern const char *target_name;  /* from magic.c */

/*
 * Perform a binary search on a section of the skill table
 * Each different section of the skill table is sorted alphabetically
 * Only match skills player knows				-Thoric
 */
bool check_skill( CHAR_DATA * ch, const char *command, const char *argument )
{
   int sn, level = 1;
   int first = gsn_first_skill;
   int top = gsn_first_weapon - 1;
   struct timeval time_used;
   int mana;

   /*
    * bsearch for the skill 
    */
   for( ;; )
   {
      sn = ( first + top ) >> 1;

      if( LOWER( command[0] ) == LOWER( skill_table[sn]->name[0] )
          && !str_prefix( command, skill_table[sn]->name )
          && ( skill_table[sn]->skill_fun || skill_table[sn]->spell_fun != spell_null )
          && ( IS_NPC( ch ) || ( ch->pcdata->learned[sn] > 0 ) ) )
         break;
      if( first >= top )
         return FALSE;
      if( strcasecmp( command, skill_table[sn]->name ) < 1 )
         top = sn - 1;
      else
         first = sn + 1;
   }

   if( !check_pos( ch, skill_table[sn]->minimum_position ) )
      return TRUE;

   if( IS_NPC( ch ) && ( IS_AFFECTED( ch, AFF_CHARM ) || IS_AFFECTED( ch, AFF_POSSESS ) ) )
   {
      send_to_char( "For some reason, you seem unable to perform that...\r\n", ch );
      act( AT_GREY, "$n looks around.", ch, NULL, NULL, TO_ROOM );
      return TRUE;
   }

   /*
    * check if mana is required 
    */
   if( skill_table[sn]->min_mana )
   {
      mana = IS_NPC( ch ) ? 0 : skill_table[sn]->min_mana;

      if( !IS_NPC( ch ) && ch->mana < mana )
      {
         send_to_char( "You need to rest before using the Force any more.\r\n", ch );
         return TRUE;
      }
   }
   else
   {
      mana = 0;
   }

   /*
    * Is this a real do-fun, or a really a spell?
    */
   if( !skill_table[sn]->skill_fun )
   {
      ch_ret retcode = rNONE;
      void *vo = NULL;
      CHAR_DATA *victim = NULL;
      OBJ_DATA *obj = NULL;

      target_name = "";

      switch ( skill_table[sn]->target )
      {
         default:
            bug( "%s: bad target for sn %d.", __func__, sn );
            send_to_char( "Something went wrong...\r\n", ch );
            return TRUE;

         case TAR_IGNORE:
            vo = NULL;
            if( argument[0] == '\0' )
            {
               if( ( victim = who_fighting( ch ) ) != NULL )
                  target_name = victim->name;
            }
            else
               target_name = argument;
            break;

         case TAR_CHAR_OFFENSIVE:
            if( argument[0] == '\0' && ( victim = who_fighting( ch ) ) == NULL )
            {
               ch_printf( ch, "%s who?\r\n", capitalize( skill_table[sn]->name ) );
               return TRUE;
            }
            else if( argument[0] != '\0' && ( victim = get_char_room( ch, argument ) ) == NULL )
            {
               send_to_char( "They aren't here.\r\n", ch );
               return TRUE;
            }
            if( is_safe( ch, victim ) )
               return TRUE;
            vo = ( void * )victim;
            break;

         case TAR_CHAR_DEFENSIVE:
            if( argument[0] != '\0' && ( victim = get_char_room( ch, argument ) ) == NULL )
            {
               send_to_char( "They aren't here.\r\n", ch );
               return TRUE;
            }
            if( !victim )
               victim = ch;
            vo = ( void * )victim;
            break;

         case TAR_CHAR_SELF:
            vo = ( void * )ch;
            break;

         case TAR_OBJ_INV:
            if( ( obj = get_obj_carry( ch, argument ) ) == NULL )
            {
               send_to_char( "You can't find that.\r\n", ch );
               return TRUE;
            }
            vo = ( void * )obj;
            break;
      }

      /*
       * waitstate 
       */
      WAIT_STATE( ch, skill_table[sn]->beats );
      /*
       * check for failure 
       */
      if( ( number_percent(  ) + skill_table[sn]->difficulty * 5 ) > ( IS_NPC( ch ) ? 75 : ch->pcdata->learned[sn] ) )
      {
         failed_casting( skill_table[sn], ch, ( CHAR_DATA* ) vo, obj );
         learn_from_failure( ch, sn );
         if( mana )
         {
            ch->mana -= mana / 2;
         }
         return TRUE;
      }
      if( mana )
      {
         ch->mana -= mana;
      }
      start_timer( &time_used );
            level = IS_NPC(ch) ? ch->top_level
                    : ch->skill_level[skill_table[sn]->guild];

            if (!IS_NPC(ch) && skill_table[sn]->guild == FORCE_ABILITY
                    && ch->subclass == SUBCLASS_PRODIGY)
                    level += 50;


      retcode = ( *skill_table[sn]->spell_fun ) ( sn, ch->top_level, ch, vo );
      end_timer( &time_used );
      update_userec( &time_used, &skill_table[sn]->userec );

      if( retcode == rCHAR_DIED || retcode == rERROR )
         return TRUE;

      if( char_died( ch ) )
         return TRUE;

      if( retcode == rSPELL_FAILED )
      {
         learn_from_failure( ch, sn );
         retcode = rNONE;
      }
      else
         learn_from_success( ch, sn );

      if( skill_table[sn]->target == TAR_CHAR_OFFENSIVE && victim != ch && !char_died( victim ) )
      {
         CHAR_DATA *vch;
         CHAR_DATA *vch_next;

         for( vch = ch->in_room->first_person; vch; vch = vch_next )
         {
            vch_next = vch->next_in_room;
            if( victim == vch && !victim->fighting && victim->master != ch )
            {
               retcode = multi_hit( victim, ch, TYPE_UNDEFINED );
               break;
            }
         }
      }
      return TRUE;
   }

   if( mana )
   {
      ch->mana -= mana;
   }
   ch->prev_cmd = ch->last_cmd;  /* haus, for automapping */
   ch->last_cmd = skill_table[sn]->skill_fun;
   start_timer( &time_used );
   ( *skill_table[sn]->skill_fun ) ( ch, argument );
   end_timer( &time_used );
   update_userec( &time_used, &skill_table[sn]->userec );

   tail_chain(  );
   return TRUE;
}

/*
 * Lookup a skills information
 * High god command
 */
void do_slookup( CHAR_DATA * ch, const char *argument )
{
   char buf[MAX_STRING_LENGTH];
   char arg[MAX_INPUT_LENGTH];
   int sn;
   SKILLTYPE *skill = NULL;

   one_argument( argument, arg );
   if( arg[0] == '\0' )
   {
      send_to_char( "Slookup what?\r\n", ch );
      return;
   }

   if( !str_cmp( arg, "all" ) )
   {
      for( sn = 0; sn < top_sn && skill_table[sn] && skill_table[sn]->name; sn++ )
         pager_printf( ch, "Sn: %4d Slot: %4d Skill/spell: '%-20s' Damtype: %s\r\n",
                       sn, skill_table[sn]->slot, skill_table[sn]->name, spell_damage[SPELL_DAMAGE( skill_table[sn] )] );
   }
   else if( !str_cmp( arg, "herbs" ) )
   {
      for( sn = 0; sn < top_herb && herb_table[sn] && herb_table[sn]->name; sn++ )
         pager_printf( ch, "%d) %s\r\n", sn, herb_table[sn]->name );
   }
   else
   {
      SMAUG_AFF *aff;
      int cnt = 0;

      if( arg[0] == 'h' && is_number( arg + 1 ) )
      {
         sn = atoi( arg + 1 );
         if( !IS_VALID_HERB( sn ) )
         {
            send_to_char( "Invalid herb.\r\n", ch );
            return;
         }
         skill = herb_table[sn];
      }
      else if( is_number( arg ) )
      {
         sn = atoi( arg );
         if( ( skill = get_skilltype( sn ) ) == NULL )
         {
            send_to_char( "Invalid sn.\r\n", ch );
            return;
         }
         sn %= 1000;
      }
      else if( ( sn = skill_lookup( arg ) ) >= 0 )
         skill = skill_table[sn];
      else if( ( sn = herb_lookup( arg ) ) >= 0 )
         skill = herb_table[sn];
      else
      {
         send_to_char( "No such skill, spell, proficiency or tongue.\r\n", ch );
         return;
      }
      if( !skill )
      {
         send_to_char( "Not created yet.\r\n", ch );
         return;
      }

      ch_printf( ch, "Sn: %4d Slot: %4d %s: '%-20s'\r\n", sn, skill->slot, skill_tname[skill->type], skill->name );
      if( skill->flags )
      {
         int x;

         ch_printf( ch, "Damtype: %s  Acttype: %s   Classtype: %s   Powertype: %s\r\n",
                    spell_damage[SPELL_DAMAGE( skill )],
                    spell_action[SPELL_ACTION( skill )],
                    spell_class[SPELL_CLASS( skill )], spell_power[SPELL_POWER( skill )] );
         mudstrlcpy( buf, "Flags:", MAX_STRING_LENGTH );
         for( x = 11; x < 32; x++ )
            if( SPELL_FLAG( skill, 1 << x ) )
            {
               mudstrlcat( buf, " ", MAX_STRING_LENGTH );
               mudstrlcat( buf, spell_flag[x - 11], MAX_STRING_LENGTH );
            }
         mudstrlcat( buf, "\r\n", MAX_STRING_LENGTH );
         send_to_char( buf, ch );
      }
      ch_printf( ch, "Saves: %s\r\n", spell_saves[( int )skill->saves] );

      if( skill->difficulty != '\0' )
         ch_printf( ch, "Difficulty: %d\r\n", ( int )skill->difficulty );

      ch_printf( ch, "Type: %s  Target: %s  Minpos: %d  Mana: %d  Beats: %d\r\n",
                 skill_tname[skill->type],
                 target_type[URANGE( TAR_IGNORE, skill->target, TAR_OBJ_INV )],
                 skill->minimum_position, skill->min_mana, skill->beats );
      ch_printf( ch, "Flags: %d  Guild: %d  Code: %s\r\n",
                 skill->flags,
                 skill->guild, skill->skill_fun ? skill->skill_fun_name : skill->spell_fun_name );
      ch_printf( ch, "Dammsg: %s\r\nWearoff: %s\n", skill->noun_damage, skill->msg_off ? skill->msg_off : "(none set)" );
      if( skill->dice && skill->dice[0] != '\0' )
         ch_printf( ch, "Dice: %s\r\n", skill->dice );
      if( skill->teachers && skill->teachers[0] != '\0' )
         ch_printf( ch, "Teachers: %s\r\n", skill->teachers );
      if( skill->components && skill->components[0] != '\0' )
         ch_printf( ch, "Components: %s\r\n", skill->components );
      if( skill->participants )
         ch_printf( ch, "Participants: %d\r\n", ( int )skill->participants );
      if( skill->userec.num_uses )
         send_timer( &skill->userec, ch );
      for( aff = skill->first_affect; aff; aff = aff->next )
      {
         if( aff == skill->first_affect )
            send_to_char( "\r\n", ch );
         snprintf( buf, MAX_STRING_LENGTH, "Affect %d", ++cnt );
         if( aff->location )
         {
            mudstrlcat( buf, " modifies ", MAX_STRING_LENGTH );
            mudstrlcat( buf, a_types[aff->location % REVERSE_APPLY], MAX_STRING_LENGTH );
            mudstrlcat( buf, " by '", MAX_STRING_LENGTH );
            mudstrlcat( buf, aff->modifier, MAX_STRING_LENGTH );
            if( aff->bitvector )
               mudstrlcat( buf, "' and", MAX_STRING_LENGTH );
            else
               mudstrlcat( buf, "'", MAX_STRING_LENGTH );
         }
         if( aff->bitvector )
         {
            int x;

            mudstrlcat( buf, " applies", MAX_STRING_LENGTH );
            for( x = 0; x < 32; x++ )
               if( IS_SET( aff->bitvector, 1 << x ) )
               {
                  mudstrlcat( buf, " ", MAX_STRING_LENGTH );
                  mudstrlcat( buf, a_flags[x], MAX_STRING_LENGTH );
               }
         }
         if( aff->duration[0] != '\0' && aff->duration[0] != '0' )
         {
            mudstrlcat( buf, " for '", MAX_STRING_LENGTH );
            mudstrlcat( buf, aff->duration, MAX_STRING_LENGTH );
            mudstrlcat( buf, "' rounds", MAX_STRING_LENGTH );
         }
         if( aff->location >= REVERSE_APPLY )
            mudstrlcat( buf, " (affects caster only)", MAX_STRING_LENGTH );
         mudstrlcat( buf, "\r\n", MAX_STRING_LENGTH );
         send_to_char( buf, ch );
         if( !aff->next )
            send_to_char( "\r\n", ch );
      }
      if( skill->hit_char && skill->hit_char[0] != '\0' )
         ch_printf( ch, "Hitchar   : %s\r\n", skill->hit_char );
      if( skill->hit_vict && skill->hit_vict[0] != '\0' )
         ch_printf( ch, "Hitvict   : %s\r\n", skill->hit_vict );
      if( skill->hit_room && skill->hit_room[0] != '\0' )
         ch_printf( ch, "Hitroom   : %s\r\n", skill->hit_room );
      if( skill->miss_char && skill->miss_char[0] != '\0' )
         ch_printf( ch, "Misschar  : %s\r\n", skill->miss_char );
      if( skill->miss_vict && skill->miss_vict[0] != '\0' )
         ch_printf( ch, "Missvict  : %s\r\n", skill->miss_vict );
      if( skill->miss_room && skill->miss_room[0] != '\0' )
         ch_printf( ch, "Missroom  : %s\r\n", skill->miss_room );
      if( skill->die_char && skill->die_char[0] != '\0' )
         ch_printf( ch, "Diechar   : %s\r\n", skill->die_char );
      if( skill->die_vict && skill->die_vict[0] != '\0' )
         ch_printf( ch, "Dievict   : %s\r\n", skill->die_vict );
      if( skill->die_room && skill->die_room[0] != '\0' )
         ch_printf( ch, "Dieroom   : %s\r\n", skill->die_room );
      if( skill->imm_char && skill->imm_char[0] != '\0' )
         ch_printf( ch, "Immchar   : %s\r\n", skill->imm_char );
      if( skill->imm_vict && skill->imm_vict[0] != '\0' )
         ch_printf( ch, "Immvict   : %s\r\n", skill->imm_vict );
      if( skill->imm_room && skill->imm_room[0] != '\0' )
         ch_printf( ch, "Immroom   : %s\r\n", skill->imm_room );
      if( skill->type != SKILL_HERB && skill->guild >= 0 && skill->guild < MAX_ABILITY )
      {
         ch_printf( ch, "guild: %s   Align: %4d   lvl: %3d\r\n",
                  ability_name[skill->guild], skill->alignment, skill->min_level );
      }
      send_to_char( "\r\n", ch );
   }
}

/*
 * Set a skill's attributes or what skills a player has.
 * High god command, with support for creating skills/spells/herbs/etc
 */
void do_sset( CHAR_DATA * ch, const char *argument )
{
   char arg1[MAX_INPUT_LENGTH];
   char arg2[MAX_INPUT_LENGTH];
   CHAR_DATA *victim;
   int value;
   int sn;
   bool fAll;

   argument = one_argument( argument, arg1 );
   argument = one_argument( argument, arg2 );

   if( arg1[0] == '\0' || arg2[0] == '\0' || argument[0] == '\0' )
   {
      send_to_char( "Syntax: sset <victim> <skill> <value>\r\n", ch );
      send_to_char( "or:     sset <victim> all     <value>\r\n", ch );
      if( get_trust( ch ) > LEVEL_SUB_IMPLEM )
      {
         send_to_char( "or:     sset save skill table\r\n", ch );
         send_to_char( "or:     sset save herb table\r\n", ch );
         send_to_char( "or:     sset create skill 'new skill'\r\n", ch );
         send_to_char( "or:     sset create herb 'new herb'\r\n", ch );
      }
      if( get_trust( ch ) > LEVEL_GREATER )
      {
         send_to_char( "or:     sset <sn>     <field> <value>\r\n", ch );
         send_to_char( "\r\nField being one of:\r\n", ch );
         send_to_char( "  name code target minpos slot mana beats dammsg wearoff guild minlevel\r\n", ch );
         send_to_char( "  type damtype acttype classtype powertype flag dice value difficulty affect\r\n", ch );
         send_to_char( "  rmaffect level adept hit miss die imm (char/vict/room)\r\n", ch );
         send_to_char( "  components teachers\r\n", ch );
         send_to_char( "Affect having the fields: <location> <modfifier> [duration] [bitvector]\r\n", ch );
         send_to_char( "(See AFFECTTYPES for location, and AFFECTED_BY for bitvector)\r\n", ch );
      }
      send_to_char( "Skill being any skill or spell.\r\n", ch );
      return;
   }

   if( get_trust( ch ) > LEVEL_SUB_IMPLEM && !str_cmp( arg1, "save" ) && !str_cmp( argument, "table" ) )
   {
      if( !str_cmp( arg2, "skill" ) )
      {
         send_to_char( "Saving skill table...\r\n", ch );
         save_skill_table(  );
         return;
      }
      if( !str_cmp( arg2, "herb" ) )
      {
         send_to_char( "Saving herb table...\r\n", ch );
         save_herb_table(  );
         return;
      }
   }
   if( get_trust( ch ) > LEVEL_SUB_IMPLEM
       && !str_cmp( arg1, "create" ) && ( !str_cmp( arg2, "skill" ) || !str_cmp( arg2, "herb" ) ) )
   {
      struct skill_type *skill;
      short type = SKILL_UNKNOWN;

      if( !str_cmp( arg2, "herb" ) )
      {
         type = SKILL_HERB;
         if( top_herb >= MAX_HERB )
         {
            ch_printf( ch, "The current top herb is %d, which is the maximum.  "
                       "To add more herbs,\r\nMAX_HERB will have to be "
                       "raised in mud.h, and the mud recompiled.\r\n", top_sn );
            return;
         }
      }
      else if( top_sn >= MAX_SKILL )
      {
         ch_printf( ch, "The current top sn is %d, which is the maximum.  "
                    "To add more skills,\r\nMAX_SKILL will have to be "
                    "raised in mud.h, and the mud recompiled.\r\n", top_sn );
         return;
      }
      CREATE( skill, struct skill_type, 1 );
      if( type == SKILL_HERB )
      {
         int max, x;

         herb_table[top_herb++] = skill;
         for( max = x = 0; x < top_herb - 1; x++ )
            if( herb_table[x] && herb_table[x]->slot > max )
               max = herb_table[x]->slot;
         skill->slot = max + 1;
      }
      else
         skill_table[top_sn++] = skill;
      skill->name = str_dup( argument );
      skill->noun_damage = str_dup( "" );
      skill->msg_off = str_dup( "" );
      skill->spell_fun = spell_smaug;
      skill->type = type;
      send_to_char( "Done.\r\n", ch );
      return;
   }

   if( arg1[0] == 'h' )
      sn = atoi( arg1 + 1 );
   else
      sn = atoi( arg1 );
   if( get_trust( ch ) > LEVEL_GREATER
       && ( ( arg1[0] == 'h' && is_number( arg1 + 1 ) && ( sn = atoi( arg1 + 1 ) ) >= 0 )
            || ( is_number( arg1 ) && ( sn = atoi( arg1 ) ) >= 0 ) ) )
   {
      struct skill_type *skill;

      if( arg1[0] == 'h' )
      {
         if( sn >= top_herb )
         {
            send_to_char( "Herb number out of range.\r\n", ch );
            return;
         }
         skill = herb_table[sn];
      }
      else
      {
         if( ( skill = get_skilltype( sn ) ) == NULL )
         {
            send_to_char( "Skill number out of range.\r\n", ch );
            return;
         }
         sn %= 1000;
      }

      if( !str_cmp( arg2, "difficulty" ) )
      {
         skill->difficulty = atoi( argument );
         send_to_char( "Ok.\r\n", ch );
         return;
      }
      if( !str_cmp( arg2, "participants" ) )
      {
         skill->participants = atoi( argument );
         send_to_char( "Ok.\r\n", ch );
         return;
      }
      if( !str_cmp( arg2, "alignment" ) )
      {
         skill->alignment = atoi( argument );
         send_to_char( "Ok.\r\n", ch );
         return;
      }
      if( !str_cmp( arg2, "damtype" ) )
      {
         int x = get_sdamage( argument );

         if( x == -1 )
            send_to_char( "Not a spell damage type.\r\n", ch );
         else
         {
            SET_SDAM( skill, x );
            send_to_char( "Ok.\r\n", ch );
         }
         return;
      }
      if( !str_cmp( arg2, "acttype" ) )
      {
         int x = get_saction( argument );

         if( x == -1 )
            send_to_char( "Not a spell action type.\r\n", ch );
         else
         {
            SET_SACT( skill, x );
            send_to_char( "Ok.\r\n", ch );
         }
         return;
      }
      if( !str_cmp( arg2, "classtype" ) )
      {
         int x = get_sclass( argument );

         if( x == -1 )
            send_to_char( "Not a spell class type.\r\n", ch );
         else
         {
            SET_SCLA( skill, x );
            send_to_char( "Ok.\r\n", ch );
         }
         return;
      }
      if( !str_cmp( arg2, "powertype" ) )
      {
         int x = get_spower( argument );

         if( x == -1 )
            send_to_char( "Not a spell power type.\r\n", ch );
         else
         {
            SET_SPOW( skill, x );
            send_to_char( "Ok.\r\n", ch );
         }
         return;
      }
      if( !str_cmp( arg2, "flag" ) )
      {
         int x = get_sflag( argument );

         if( x == -1 )
            send_to_char( "Not a spell flag.\r\n", ch );
         else
         {
            TOGGLE_BIT( skill->flags, 1 << ( x + 11 ) );
            send_to_char( "Ok.\r\n", ch );
         }
         return;
      }
      if( !str_cmp( arg2, "saves" ) )
      {
         int x = get_ssave( argument );

         if( x == -1 )
            send_to_char( "Not a saving type.\r\n", ch );
         else
         {
            skill->saves = x;
            send_to_char( "Ok.\r\n", ch );
         }
         return;
      }

	if ( !str_cmp( arg2, "code" ) )
	{
	   SPELL_FUN *spellfun;
	   DO_FUN *dofun;

	   if( !str_prefix( "do_", argument ) && ( dofun = skill_function( argument ) ) != skill_notfound )
	     {
	       skill->skill_fun = dofun;
	       skill->spell_fun = NULL;
	       DISPOSE( skill->skill_fun_name );
	       skill->skill_fun_name = str_dup( argument );
	     }		
	   else if( ( spellfun = spell_function( argument ) ) != spell_notfound )
	     {
	       skill->spell_fun = spellfun;
	       skill->skill_fun = NULL;
	       DISPOSE( skill->skill_fun_name );
	       skill->spell_fun_name = str_dup( argument );
	     }
	   else if( validate_spec_fun( argument ) )
	     {
	       send_to_char( "Cannot use a spec_fun for skills or spells.\r\n", ch );
	       return;
	     }
	   else
	     {
	       send_to_char( "Not a spell or skill.\r\n", ch );
	       return;
	     }
	   send_to_char( "Ok.\r\n", ch );
	   return;
	}

      if( !str_cmp( arg2, "target" ) )
      {
         int x = get_starget( argument );

         if( x == -1 )
            send_to_char( "Not a valid target type.\r\n", ch );
         else
         {
            skill->target = x;
            send_to_char( "Ok.\r\n", ch );
         }
         return;
      }
      if( !str_cmp( arg2, "minpos" ) )
      {
         skill->minimum_position = URANGE( POS_DEAD, atoi( argument ), POS_DRAG );
         send_to_char( "Ok.\r\n", ch );
         return;
      }
      if( !str_cmp( arg2, "minlevel" ) )
      {
         skill->min_level = URANGE( 1, atoi( argument ), MAX_LEVEL );
         send_to_char( "Ok.\r\n", ch );
         return;
      }
      if( !str_cmp( arg2, "slot" ) )
      {
         skill->slot = URANGE( 0, atoi( argument ), 30000 );
         send_to_char( "Ok.\r\n", ch );
         return;
      }
      if( !str_cmp( arg2, "mana" ) )
      {
         skill->min_mana = URANGE( 0, atoi( argument ), 2000 );
         send_to_char( "Ok.\r\n", ch );
         return;
      }
      if( !str_cmp( arg2, "beats" ) )
      {
         skill->beats = URANGE( 0, atoi( argument ), 120 );
         send_to_char( "Ok.\r\n", ch );
         return;
      }
      if( !str_cmp( arg2, "guild" ) )
      {
         skill->guild = atoi( argument );
         send_to_char( "Ok.\r\n", ch );
         return;
      }
      if( !str_cmp( arg2, "value" ) )
      {
         skill->value = atoi( argument );
         send_to_char( "Ok.\r\n", ch );
         return;
      }
      if( !str_cmp( arg2, "type" ) )
      {
         skill->type = get_skill( argument );
         send_to_char( "Ok.\r\n", ch );
         return;
      }
      if( !str_cmp( arg2, "rmaffect" ) )
      {
         SMAUG_AFF *aff, *aff_next;
         int num = atoi( argument );
         int cnt = 0;

         if( !skill->first_affect )
         {
            send_to_char( "This spell has no special affects to remove.\r\n", ch );
            return;
         }
         for( aff = skill->first_affect; aff; aff = aff_next )
         {
            aff_next = aff->next;

            if( ++cnt == num )
            {
               UNLINK( aff, skill->first_affect, skill->last_affect, next, prev );
               DISPOSE( aff->duration );
               DISPOSE( aff->modifier );
               DISPOSE( aff );
               send_to_char( "Removed.\r\n", ch );
               return;
            }
         }
         send_to_char( "Not found.\r\n", ch );
         return;
      }

      /*
       * affect <location> <modifier> <duration> <bitvector>
       */
      if( !str_cmp( arg2, "affect" ) )
      {
         char location[MAX_INPUT_LENGTH];
         char modifier[MAX_INPUT_LENGTH];
         char duration[MAX_INPUT_LENGTH];
         char bitvector[MAX_INPUT_LENGTH];
         int loc, bit, tmpbit;
         SMAUG_AFF *aff;

         argument = one_argument( argument, location );
         argument = one_argument( argument, modifier );
         argument = one_argument( argument, duration );

         if( location[0] == '!' )
            loc = get_atype( location + 1 ) + REVERSE_APPLY;
         else
            loc = get_atype( location );
         if( ( loc % REVERSE_APPLY ) < 0 || ( loc % REVERSE_APPLY ) >= MAX_APPLY_TYPE )
         {
            send_to_char( "Unknown affect location.  See AFFECTTYPES.\r\n", ch );
            return;
         }
         bit = 0;
         while( argument[0] != 0 )
         {
            argument = one_argument( argument, bitvector );
            if( ( tmpbit = get_aflag( bitvector ) ) == -1 )
               ch_printf( ch, "Unknown bitvector: %s.  See AFFECTED_BY\r\n", bitvector );
            else
               bit |= ( 1 << tmpbit );
         }
         CREATE( aff, SMAUG_AFF, 1 );
         if( !str_cmp( duration, "0" ) )
            duration[0] = '\0';
         if( !str_cmp( modifier, "0" ) )
            modifier[0] = '\0';
         aff->duration = str_dup( duration );
         aff->location = loc;
         if( loc == APPLY_AFFECT || loc == APPLY_RESISTANT || loc == APPLY_IMMUNE || loc == APPLY_SUSCEPTIBLE )
         {
            int modval = get_aflag( modifier );

            /* Sanitize the flag input for the modifier if needed -- Samson */
            if( modval < 0 )
               modval = 0;
            snprintf( modifier, MAX_INPUT_LENGTH, "%d", modval );
         }
         aff->modifier = str_dup( modifier );
         aff->bitvector = bit;
         LINK( aff, skill->first_affect, skill->last_affect, next, prev );
         send_to_char( "Ok.\r\n", ch );
         return;
      }
      if( !str_cmp( arg2, "level" ) )
      {
         skill->min_level = URANGE( 1, atoi( argument ), MAX_LEVEL );
         send_to_char( "Ok.\r\n", ch );
         return;
      }
      if( !str_cmp( arg2, "adept" ) )
      {
         return;
      }
      if( !str_cmp( arg2, "name" ) )
      {
         DISPOSE( skill->name );
         skill->name = str_dup( argument );
         send_to_char( "Ok.\r\n", ch );
         return;
      }
      if( !str_cmp( arg2, "dammsg" ) )
      {
         DISPOSE( skill->noun_damage );
         if( !str_cmp( argument, "clear" ) )
            skill->noun_damage = str_dup( "" );
         else
            skill->noun_damage = str_dup( argument );
         send_to_char( "Ok.\r\n", ch );
         return;
      }
      if( !str_cmp( arg2, "wearoff" ) )
      {
         DISPOSE( skill->msg_off );
         if( str_cmp( argument, "clear" ) )
            skill->msg_off = str_dup( argument );
         send_to_char( "Ok.\r\n", ch );
         return;
      }
      if( !str_cmp( arg2, "hitchar" ) )
      {
         if( skill->hit_char )
            DISPOSE( skill->hit_char );
         if( str_cmp( argument, "clear" ) )
            skill->hit_char = str_dup( argument );
         send_to_char( "Ok.\r\n", ch );
         return;
      }
      if( !str_cmp( arg2, "hitvict" ) )
      {
         if( skill->hit_vict )
            DISPOSE( skill->hit_vict );
         if( str_cmp( argument, "clear" ) )
            skill->hit_vict = str_dup( argument );
         send_to_char( "Ok.\r\n", ch );
         return;
      }
      if( !str_cmp( arg2, "hitroom" ) )
      {
         if( skill->hit_room )
            DISPOSE( skill->hit_room );
         if( str_cmp( argument, "clear" ) )
            skill->hit_room = str_dup( argument );
         send_to_char( "Ok.\r\n", ch );
         return;
      }
      if( !str_cmp( arg2, "misschar" ) )
      {
         if( skill->miss_char )
            DISPOSE( skill->miss_char );
         if( str_cmp( argument, "clear" ) )
            skill->miss_char = str_dup( argument );
         send_to_char( "Ok.\r\n", ch );
         return;
      }
      if( !str_cmp( arg2, "missvict" ) )
      {
         if( skill->miss_vict )
            DISPOSE( skill->miss_vict );
         if( str_cmp( argument, "clear" ) )
            skill->miss_vict = str_dup( argument );
         send_to_char( "Ok.\r\n", ch );
         return;
      }
      if( !str_cmp( arg2, "missroom" ) )
      {
         if( skill->miss_room )
            DISPOSE( skill->miss_room );
         if( str_cmp( argument, "clear" ) )
            skill->miss_room = str_dup( argument );
         send_to_char( "Ok.\r\n", ch );
         return;
      }
      if( !str_cmp( arg2, "diechar" ) )
      {
         if( skill->die_char )
            DISPOSE( skill->die_char );
         if( str_cmp( argument, "clear" ) )
            skill->die_char = str_dup( argument );
         send_to_char( "Ok.\r\n", ch );
         return;
      }
      if( !str_cmp( arg2, "dievict" ) )
      {
         if( skill->die_vict )
            DISPOSE( skill->die_vict );
         if( str_cmp( argument, "clear" ) )
            skill->die_vict = str_dup( argument );
         send_to_char( "Ok.\r\n", ch );
         return;
      }
      if( !str_cmp( arg2, "dieroom" ) )
      {
         if( skill->die_room )
            DISPOSE( skill->die_room );
         if( str_cmp( argument, "clear" ) )
            skill->die_room = str_dup( argument );
         send_to_char( "Ok.\r\n", ch );
         return;
      }
      if( !str_cmp( arg2, "immchar" ) )
      {
         if( skill->imm_char )
            DISPOSE( skill->imm_char );
         if( str_cmp( argument, "clear" ) )
            skill->imm_char = str_dup( argument );
         send_to_char( "Ok.\r\n", ch );
         return;
      }
      if( !str_cmp( arg2, "immvict" ) )
      {
         if( skill->imm_vict )
            DISPOSE( skill->imm_vict );
         if( str_cmp( argument, "clear" ) )
            skill->imm_vict = str_dup( argument );
         send_to_char( "Ok.\r\n", ch );
         return;
      }
      if( !str_cmp( arg2, "immroom" ) )
      {
         if( skill->imm_room )
            DISPOSE( skill->imm_room );
         if( str_cmp( argument, "clear" ) )
            skill->imm_room = str_dup( argument );
         send_to_char( "Ok.\r\n", ch );
         return;
      }
      if( !str_cmp( arg2, "dice" ) )
      {
         if( skill->dice )
            DISPOSE( skill->dice );
         if( str_cmp( argument, "clear" ) )
            skill->dice = str_dup( argument );
         send_to_char( "Ok.\r\n", ch );
         return;
      }
      if( !str_cmp( arg2, "components" ) )
      {
         if( skill->components )
            DISPOSE( skill->components );
         if( str_cmp( argument, "clear" ) )
            skill->components = str_dup( argument );
         send_to_char( "Ok.\r\n", ch );
         return;
      }
      if( !str_cmp( arg2, "teachers" ) )
      {
         if( skill->teachers )
            DISPOSE( skill->teachers );
         if( str_cmp( argument, "clear" ) )
            skill->teachers = str_dup( argument );
         send_to_char( "Ok.\r\n", ch );
         return;
      }
      do_sset( ch, "" );
      return;
   }

   if( ( victim = get_char_world( ch, arg1 ) ) == NULL )
   {
      if( ( sn = skill_lookup( arg1 ) ) >= 0 )
      {
         char buf[MAX_STRING_LENGTH];

         snprintf( buf, MAX_STRING_LENGTH, "%d %s %s", sn, arg2, argument );
         do_sset( ch, buf );
      }
      else
         send_to_char( "They aren't here.\r\n", ch );
      return;
   }

   if( IS_NPC( victim ) )
   {
      send_to_char( "Not on NPC's.\r\n", ch );
      return;
   }

   fAll = !str_cmp( arg2, "all" );
   sn = 0;
   if( !fAll && ( sn = skill_lookup( arg2 ) ) < 0 )
   {
      send_to_char( "No such skill or spell.\r\n", ch );
      return;
   }

   /*
    * Snarf the value.
    */
   if( !is_number( argument ) )
   {
      send_to_char( "Value must be numeric.\r\n", ch );
      return;
   }

   value = atoi( argument );
   if( value < 0 || value > 100 )
   {
      send_to_char( "Value range is 0 to 100.\r\n", ch );
      return;
   }

   if( fAll )
   {
      for( sn = 0; sn < top_sn; sn++ )
      {
         /*
          * Fix by Narn to prevent ssetting skills the player shouldn't have. 
          */
         if( skill_table[sn]->guild < 0 || skill_table[sn]->guild >= MAX_ABILITY )
            continue;
         if( skill_table[sn]->name
             && ( victim->skill_level[skill_table[sn]->guild] >= skill_table[sn]->min_level || value == 0 ) )
            victim->pcdata->learned[sn] = value;
      }
   }
   else
      victim->pcdata->learned[sn] = value;
}

void learn_from_success( CHAR_DATA * ch, int sn )
{
   int adept, gain, sklvl, learn, percent, schance;
   int skill;

   if( IS_NPC( ch ) || ch->pcdata->learned[sn] == 0 )
      return;

   if( sn == skill_lookup( "meditate" ) && ch->skill_level[FORCE_ABILITY] < 2 )
      gain_exp( ch, 500, FORCE_ABILITY );

   /* changed skllvl to cap skill experience gains to level 150, in cases of skills
 * available to items only, which are above 150 to stay off of the skill list
 * -Lajos
 */
   //sklvl = skill_table[sn]->min_level;
   sklvl = URANGE(1, skill_table[sn]->min_level, 150);
  
   if( skill_table[sn]->guild < 0 || skill_table[sn]->guild >= MAX_ABILITY )
      return;

   adept = ( ch->skill_level[skill_table[sn]->guild] - skill_table[sn]->min_level ) * 5 + 50;
   
   
   /*adept = UMIN( adept, 100 ); */
   adept = 100;

   /* In the old skill code, it was possible to train past perfect without adjusting the adept
     * because skill bonuses were applied directly to skills. This is no longer the case.
     * However, I find that training past perfect is a valuable feature, so I'm going to include
     * it here by adjusting the adept based on the item bonus received. -Lajos
     */
   if (ch->subclass == SUBCLASS_BLADEMASTER && (sn == gsn_swords || sn == gsn_lightsabers))
   {
       adept = UMAX(adept, 150);
   }
   if (ch->subclass == SUBCLASS_SITH_HUNTER)
   {
       if (sn == gsn_lightsabers || sn == gsn_parry)
           adept = 200;
       else if (sn == gsn_lightsaber_crafting || sn == gsn_doublelightsaber_crafting || sn == gsn_disarm)
           adept = 110;
   }

   /*
   if (ch->subclass == SUBCLASS_MERCENARY)
   {
       if (sn == gsn_spray)
           adept = 150;
   } */

   adept -= ch->pcdata->item_bonus[sn];

   if (ch->pcdata->learned[sn] >= adept)
       return;

   if( sklvl == 0 || sklvl > ch->skill_level[skill_table[sn]->guild] )
      sklvl = ch->skill_level[skill_table[sn]->guild];

   if( ch->pcdata->learned[sn] < adept )
   {
      skill = ch->pcdata->learned[sn];
      schance = ch->pcdata->learned[sn] + ( 5 * skill_table[sn]->difficulty );
      percent = number_percent(  );
      if (adept > 100) /* Give people a fair chance for training past perfect */
      {
          while (skill > 100)
          {
              percent += number_percent();
              skill -= 100;
          }
      }
      
      if( percent >= schance )
         learn = 2;
      else if( schance - percent > 25 )
         return;
      else
         learn = 1;
      ch->pcdata->learned[sn] = UMIN( adept, ch->pcdata->learned[sn] + learn );
      if( ch->pcdata->learned[sn] == 100 )   /* fully learned! */
      {
         gain = 50 * sklvl;
         set_char_color( AT_WHITE, ch );
         ch_printf( ch, "You are now an adept of %s!  You gain %d bonus experience!\r\n", skill_table[sn]->name, gain );
      }
      else
      {
         gain = 10 * sklvl;
         if( !ch->fighting && sn != gsn_hide && sn != gsn_sneak )
         {
            set_char_color( AT_WHITE, ch );
            ch_printf( ch, "You gain %d experience points from your success!\r\n", gain );
         }
      }
      gain_exp( ch, gain, skill_table[sn]->guild );
   }
}

void learn_from_failure( CHAR_DATA * ch, int sn )
{
}

void do_gouge( CHAR_DATA * ch, const char *argument )
{
   CHAR_DATA *victim;
   AFFECT_DATA af;
   short dam;
   int percent;

   if (!IS_NPC(ch))
   {
       if (IS_SET(ch->pcdata->flags, PCFLAG_BOUND))
       {
           send_to_char("How do you intend to do that when bound?\n\r", ch);
           return;
       }
   }


   if( IS_NPC( ch ) && IS_AFFECTED( ch, AFF_CHARM ) )
   {
      send_to_char( "You can't concentrate enough for that.\r\n", ch );
      return;
   }

   if( !IS_NPC( ch ) && !ch->pcdata->learned[gsn_gouge] )
   {
      send_to_char( "You do not yet know of this skill.\r\n", ch );
      return;
   }

   if( ch->mount )
   {
      send_to_char( "You can't get close enough while mounted.\r\n", ch );
      return;
   }

   if( ( victim = who_fighting( ch ) ) == NULL )
   {
      send_to_char( "You aren't fighting anyone.\r\n", ch );
      return;
   }

   percent = number_percent(  ) - ( get_curr_lck( ch ) - 13 );

   if( IS_NPC( ch ) || percent < ch->pcdata->learned[gsn_gouge] )
   {
      dam = number_range( 1, ch->skill_level[COMBAT_ABILITY] );
      global_retcode = damage( ch, victim, dam, gsn_gouge );
      if( global_retcode == rNONE )
      {
         if( !IS_AFFECTED( victim, AFF_BLIND ) )
         {
            af.type = gsn_blindness;
            af.location = APPLY_HITROLL;
            af.modifier = -6;
            af.duration = 3 + ( ch->skill_level[COMBAT_ABILITY] / 20 );
            af.bitvector = AFF_BLIND;
            affect_to_char( victim, &af );
            act( AT_SKILL, "You can't see a thing!", victim, NULL, NULL, TO_CHAR );
         }
         WAIT_STATE( ch, PULSE_VIOLENCE );
         WAIT_STATE( victim, PULSE_VIOLENCE );
         /*
          * Taken out by request - put back in by Thoric
          * * This is how it was designed.  You'd be a tad stunned
          * * if someone gouged you in the eye.
          */
      }
      else if( global_retcode == rVICT_DIED )
      {
         act( AT_BLOOD, "Your fingers plunge into your victim's brain, causing immediate death!", ch, NULL, NULL, TO_CHAR );
      }
      if( global_retcode != rCHAR_DIED && global_retcode != rBOTH_DIED )
         learn_from_success( ch, gsn_gouge );
   }
   else
   {
      WAIT_STATE( ch, skill_table[gsn_gouge]->beats );
      global_retcode = damage( ch, victim, 0, gsn_gouge );
      learn_from_failure( ch, gsn_gouge );
   }
}

void do_detrap( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   OBJ_DATA *obj;
   OBJ_DATA *trap;
   int percent;
   bool found = FALSE;

   if (!IS_NPC(ch))
   {
       if (IS_SET(ch->pcdata->flags, PCFLAG_BOUND))
       {
           send_to_char("How do you intend to do that when bound?\n\r", ch);
           return;
       }
   }


   switch ( ch->substate )
   {
      default:
         if( IS_NPC( ch ) && IS_AFFECTED( ch, AFF_CHARM ) )
         {
            send_to_char( "You can't concentrate enough for that.\r\n", ch );
            return;
         }
         argument = one_argument( argument, arg );
         if( !IS_NPC( ch ) && !ch->pcdata->learned[gsn_detrap] )
         {
            send_to_char( "You do not yet know of this skill.\r\n", ch );
            return;
         }
         if( arg[0] == '\0' )
         {
            send_to_char( "Detrap what?\r\n", ch );
            return;
         }
         if( ms_find_obj( ch ) )
            return;
         found = FALSE;
         if( ch->mount )
         {
            send_to_char( "You can't do that while mounted.\r\n", ch );
            return;
         }
         if( !ch->in_room->first_content )
         {
            send_to_char( "You can't find that here.\r\n", ch );
            return;
         }
         for( obj = ch->in_room->first_content; obj; obj = obj->next_content )
         {
            if( can_see_obj( ch, obj ) && nifty_is_name( arg, obj->name ) )
            {
               found = TRUE;
               break;
            }
         }
         if( !found )
         {
            send_to_char( "You can't find that here.\r\n", ch );
            return;
         }
         act( AT_ACTION, "You carefully begin your attempt to remove a trap from $p...", ch, obj, NULL, TO_CHAR );
         act( AT_ACTION, "$n carefully attempts to remove a trap from $p...", ch, obj, NULL, TO_ROOM );
         ch->dest_buf = str_dup( obj->name );
         add_timer( ch, TIMER_DO_FUN, 3, do_detrap, 1 );
/*	    WAIT_STATE( ch, skill_table[gsn_detrap]->beats ); */
         return;
      case 1:
         if( !ch->dest_buf )
         {
            send_to_char( "Your detrapping was interrupted!\r\n", ch );
            bug( "%s: ch->dest_buf NULL!", __func__ );
            return;
         }
         mudstrlcpy( arg, ( const char* ) ch->dest_buf, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf );
         ch->dest_buf = NULL;
         ch->substate = SUB_NONE;
         break;
      case SUB_TIMER_DO_ABORT:
         DISPOSE( ch->dest_buf );
         ch->substate = SUB_NONE;
         send_to_char( "You carefully stop what you were doing.\r\n", ch );
         return;
   }

   if( !ch->in_room->first_content )
   {
      send_to_char( "You can't find that here.\r\n", ch );
      return;
   }
   for( obj = ch->in_room->first_content; obj; obj = obj->next_content )
   {
      if( can_see_obj( ch, obj ) && nifty_is_name( arg, obj->name ) )
      {
         found = TRUE;
         break;
      }
   }
   if( !found )
   {
      send_to_char( "You can't find that here.\r\n", ch );
      return;
   }
   if( ( trap = get_trap( obj ) ) == NULL )
   {
      send_to_char( "You find no trap on that.\r\n", ch );
      return;
   }

   percent = number_percent(  ) - ( ch->skill_level[SMUGGLING_ABILITY] / 20 ) - ( get_curr_lck( ch ) - 16 );

   separate_obj( obj );
   if( !IS_NPC( ch ) || percent > ch->pcdata->learned[gsn_detrap] )
   {
      send_to_char( "Ooops!\r\n", ch );
      spring_trap( ch, trap );
      learn_from_failure( ch, gsn_detrap );
      return;
   }

   extract_obj( trap );

   send_to_char( "You successfully remove a trap.\r\n", ch );
   learn_from_success( ch, gsn_detrap );
}

void do_dig( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   OBJ_DATA *obj;
   OBJ_DATA *startobj;
   bool found, shovel;
   EXIT_DATA *pexit;

   if (!IS_NPC(ch))
   {
       if (IS_SET(ch->pcdata->flags, PCFLAG_BOUND))
       {
           send_to_char("How do you intend to do that when bound?\n\r", ch);
           return;
       }
   }


   switch ( ch->substate )
   {
      default:
         if( IS_NPC( ch ) && IS_AFFECTED( ch, AFF_CHARM ) )
         {
            send_to_char( "You can't concentrate enough for that.\r\n", ch );
            return;
         }
         if( ch->mount )
         {
            send_to_char( "You can't do that while mounted.\r\n", ch );
            return;
         }
         one_argument( argument, arg );
         if( arg[0] != '\0' )
         {
            if( ( pexit = find_door( ch, arg, TRUE ) ) == NULL && get_dir( arg ) == -1 )
            {
               send_to_char( "What direction is that?\r\n", ch );
               return;
            }
            if( pexit )
            {
               if( !IS_SET( pexit->exit_info, EX_DIG ) && !IS_SET( pexit->exit_info, EX_CLOSED ) )
               {
                  send_to_char( "There is no need to dig out that exit.\r\n", ch );
                  return;
               }
            }
         }
         else
         {
            switch ( ch->in_room->sector_type )
            {
               case SECT_CITY:
               case SECT_INSIDE:
                  send_to_char( "The floor is too hard to dig through.\r\n", ch );
                  return;
               case SECT_WATER_SWIM:
               case SECT_WATER_NOSWIM:
               case SECT_UNDERWATER:
                  send_to_char( "You cannot dig here.\r\n", ch );
                  return;
               case SECT_AIR:
                  send_to_char( "What?  In the air?!\r\n", ch );
                  return;
            }
         }
         add_timer( ch, TIMER_DO_FUN, UMIN( skill_table[gsn_dig]->beats / 10, 3 ), do_dig, 1 );
         ch->dest_buf = str_dup( arg );
         send_to_char( "You begin digging...\r\n", ch );
         act( AT_PLAIN, "$n begins digging...", ch, NULL, NULL, TO_ROOM );
         return;

      case 1:
         if( !ch->dest_buf )
         {
            send_to_char( "Your digging was interrupted!\r\n", ch );
            act( AT_PLAIN, "$n's digging was interrupted!", ch, NULL, NULL, TO_ROOM );
            bug( "%s: dest_buf NULL", __func__ );
            return;
         }
         mudstrlcpy( arg, ( const char* ) ch->dest_buf, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf );
         break;

      case SUB_TIMER_DO_ABORT:
         DISPOSE( ch->dest_buf );
         ch->substate = SUB_NONE;
         send_to_char( "You stop digging...\r\n", ch );
         act( AT_PLAIN, "$n stops digging...", ch, NULL, NULL, TO_ROOM );
         return;
   }

   ch->substate = SUB_NONE;

   /*
    * not having a shovel makes it harder to succeed 
    */
   shovel = FALSE;
   for( obj = ch->first_carrying; obj; obj = obj->next_content )
      if( obj->item_type == ITEM_SHOVEL )
      {
         shovel = TRUE;
         break;
      }

   /*
    * dig out an EX_DIG exit... 
    */
   if( arg[0] != '\0' )
   {
      if( ( pexit = find_door( ch, arg, TRUE ) ) != NULL
          && IS_SET( pexit->exit_info, EX_DIG ) && IS_SET( pexit->exit_info, EX_CLOSED ) )
      {
         /*
          * 4 times harder to dig open a passage without a shovel 
          */
         if( ( number_percent(  ) * ( shovel ? 1 : 4 ) ) < ( IS_NPC( ch ) ? 80 : ch->pcdata->learned[gsn_dig] ) )
         {
            REMOVE_BIT( pexit->exit_info, EX_CLOSED );
            send_to_char( "You dig open a passageway!\r\n", ch );
            act( AT_PLAIN, "$n digs open a passageway!", ch, NULL, NULL, TO_ROOM );
            learn_from_success( ch, gsn_dig );
            return;
         }
      }
      learn_from_failure( ch, gsn_dig );
      send_to_char( "Your dig did not discover any exit...\r\n", ch );
      act( AT_PLAIN, "$n's dig did not discover any exit...", ch, NULL, NULL, TO_ROOM );
      return;
   }

   startobj = ch->in_room->first_content;
   found = FALSE;

   for( obj = startobj; obj; obj = obj->next_content )
   {
      /*
       * twice as hard to find something without a shovel 
       */
      if( IS_OBJ_STAT( obj, ITEM_BURRIED )
          && ( number_percent(  ) * ( shovel ? 1 : 2 ) ) < ( IS_NPC( ch ) ? 80 : ch->pcdata->learned[gsn_dig] ) )
      {
         found = TRUE;
         break;
      }
   }

   if( !found )
   {
      send_to_char( "Your dig uncovered nothing.\r\n", ch );
      act( AT_PLAIN, "$n's dig uncovered nothing.", ch, NULL, NULL, TO_ROOM );
      learn_from_failure( ch, gsn_dig );
      return;
   }

   separate_obj( obj );
   REMOVE_BIT( obj->extra_flags, ITEM_BURRIED );
   act( AT_SKILL, "Your dig uncovered $p!", ch, obj, NULL, TO_CHAR );
   act( AT_SKILL, "$n's dig uncovered $p!", ch, obj, NULL, TO_ROOM );
   learn_from_success( ch, gsn_dig );
}

void do_search( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   OBJ_DATA *obj = NULL;
   OBJ_DATA *container;
   OBJ_DATA *startobj;
   int percent, door;
   bool found;

   if (!IS_NPC(ch))
   {
       if (IS_SET(ch->pcdata->flags, PCFLAG_BOUND))
       {
           send_to_char("How do you intend to do that when bound?\n\r", ch);
           return;
       }
   }


   door = -1;
   switch ( ch->substate )
   {
      default:
         if( IS_NPC( ch ) && IS_AFFECTED( ch, AFF_CHARM ) )
         {
            send_to_char( "You can't concentrate enough for that.\r\n", ch );
            return;
         }
         if( ch->mount )
         {
            send_to_char( "You can't do that while mounted.\r\n", ch );
            return;
         }
         argument = one_argument( argument, arg );
         if( arg[0] != '\0' && ( door = get_door( arg ) ) == -1 )
         {
            container = get_obj_here( ch, arg );
            if( !container )
            {
               send_to_char( "You can't find that here.\r\n", ch );
               return;
            }
            if( container->item_type != ITEM_CONTAINER )
            {
               send_to_char( "You can't search in that!\r\n", ch );
               return;
            }
            if( IS_SET( container->value[1], CONT_CLOSED ) )
            {
               send_to_char( "It is closed.\r\n", ch );
               return;
            }
         }
         add_timer( ch, TIMER_DO_FUN, UMIN( skill_table[gsn_search]->beats / 10, 3 ), do_search, 1 );
         send_to_char( "You begin your search...\r\n", ch );
         ch->dest_buf = str_dup( arg );
         return;

      case 1:
         if( !ch->dest_buf )
         {
            send_to_char( "Your search was interrupted!\r\n", ch );
            bug( "%s: dest_buf NULL", __func__ );
            return;
         }
         mudstrlcpy( arg, ( const char* ) ch->dest_buf, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf );
         break;
      case SUB_TIMER_DO_ABORT:
         DISPOSE( ch->dest_buf );
         ch->substate = SUB_NONE;
         send_to_char( "You stop your search...\r\n", ch );
         return;
   }
   ch->substate = SUB_NONE;
   if( arg[0] == '\0' )
   {
      startobj = ch->in_room->first_content;
   }
   else
   {
      if( ( door = get_door( arg ) ) != -1 )
         startobj = NULL;
      else
      {
         container = get_obj_here( ch, arg );
         if( !container )
         {
            send_to_char( "You can't find that here.\r\n", ch );
            return;
         }
         startobj = container->first_content;
      }
   }

   found = FALSE;

   if( ( !startobj && door == -1 ) || IS_NPC( ch ) )
   {
      send_to_char( "You find nothing.\r\n", ch );
      learn_from_failure( ch, gsn_search );
      return;
   }

   percent = number_percent(  );

   if( door != -1 )
   {
      EXIT_DATA *pexit;

      if( ( pexit = get_exit( ch->in_room, door ) ) != NULL
          && IS_SET( pexit->exit_info, EX_SECRET )
          && IS_SET( pexit->exit_info, EX_xSEARCHABLE )
          && percent < ( IS_NPC( ch ) ? 80 : ch->pcdata->learned[gsn_search] ) )
      {
         act( AT_SKILL, "Your search reveals the $d!", ch, NULL, pexit->keyword, TO_CHAR );
         act( AT_SKILL, "$n finds the $d!", ch, NULL, pexit->keyword, TO_ROOM );
         REMOVE_BIT( pexit->exit_info, EX_SECRET );
         learn_from_success( ch, gsn_search );
         return;
      }
   }
   else
      for( obj = startobj; obj; obj = obj->next_content )
      {
         if( IS_OBJ_STAT( obj, ITEM_HIDDEN ) && percent < ch->pcdata->learned[gsn_search] )
         {
            found = TRUE;
            break;
         }
      }

   if( !found )
   {
      send_to_char( "You find nothing.\r\n", ch );
      learn_from_failure( ch, gsn_search );
      return;
   }

   separate_obj( obj );
   REMOVE_BIT( obj->extra_flags, ITEM_HIDDEN );
   act( AT_SKILL, "Your search reveals $p!", ch, obj, NULL, TO_CHAR );
   act( AT_SKILL, "$n finds $p!", ch, obj, NULL, TO_ROOM );
   learn_from_success( ch, gsn_search );
}


void do_steal( CHAR_DATA * ch, const char *argument )
{
    char logbuf[MAX_STRING_LENGTH];
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    CHAR_DATA* victim, * mst;
    OBJ_DATA* obj, * obj_next;
    int percent, xp, stealChance;
    OBJ_DATA* stolen_obj;
    AFFECT_DATA af;

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);

    if (ch->mount)
    {
        send_to_char("You can't do that while mounted.\n\r", ch);
        return;
    }

    if (arg1[0] == '\0' || arg2[0] == '\0')
    {
        send_to_char("Steal what from whom?\n\r", ch);
        return;
    }

    if (ms_find_obj(ch))
        return;

    if ((victim = get_char_room(ch, arg2)) == NULL )
    {
        send_to_char("They aren't here.\n\r", ch);
        return;
    }

    if (victim == ch)
    {
        send_to_char("That's pointless.\n\r", ch);
        return;
    }

    if (check_illegal_psteal(ch, victim))
    {
        send_to_char("You can't steal from that player.\n\r", ch);
        return;
    }

    WAIT_STATE(ch, skill_table[gsn_steal]->beats);
    percent = number_percent();
    //stealChance = number_percent() + ((get_skill_percent(ch, gsn_steal)) / 2);
    stealChance = number_percent() + (((ch->pcdata->learned[gsn_steal])) / 2);
    /* 75% cap for just the skill bonus */
    stealChance = UMIN(stealChance, 75);

    /* Apply other possible bonuses */
    if (IS_AWAKE(victim)) // awareness bonus
    {
        if (!can_see(victim, ch))
            percent -= 50; // vision penalty
        if (IS_AFFECTED(victim, AFF_SIT_AWARE))
            percent += 50;
        if (who_fighting(victim) != NULL)
            percent += 25;
        if (IS_NPC(victim) && (is_hating(ch, victim) || is_hunting(ch, victim) || is_fearing(ch, victim)))
            percent += 25; /* Hostile mobs get a bonus to detection */
    }
    else
        percent -= 50; // vision penalty
    percent += victim->perm_frc + (get_curr_wis(victim) + get_curr_lck(victim)) / 2; // perception bonus

    if (IS_NPC(victim) && !IS_AFFECTED(victim, AFF_CHARM) && victim->pIndexData->pShop != NULL)
        percent += 25; /* Make shops harder to steal from */

    if (ch->subclass == SUBCLASS_SNEAK)  //Subclass bonus -- 50%!
        stealChance += 50;
    stealChance = UMIN(stealChance, 95);

    sprintf(logbuf, "%s stealing from %s (%d/%d)", ch->name, victim->name, percent, stealChance);
    log_string(logbuf);
    if (percent >= stealChance)
    {
        /*
         * Failure.
         */
        act(AT_ACTION, "Ahh shit....\n\r", ch, NULL, victim, TO_CHAR);
        act(AT_ACTION, "You catch $n's fingers in your pocket.", ch, NULL, victim, TO_VICT);
        act(AT_ACTION, "$N catches $n's fingers in $S pocket.", ch, NULL, victim, TO_NOTVICT);

        if (!IS_AFFECTED(victim, AFF_SIT_AWARE))
        {
            af.type = gsn_aware;
            af.location = APPLY_AC;
            af.modifier = -10;
            af.duration = URANGE(100, victim->top_level * 5 + 30, 300);
            af.bitvector = AFF_SIT_AWARE;
            affect_to_char(victim, &af);
        }

        learn_from_failure(ch, gsn_steal);
        if (!IS_NPC(ch))
        {
            if (legal_loot(ch, victim))
            {
                if (IS_NPC(victim))
                    global_retcode = multi_hit(victim, ch, TYPE_UNDEFINED);
            }
            else
            {
                if (IS_NPC(ch))
                {
                    if ((mst = ch->master) == NULL)
                        return;
                }
                else
                    mst = ch;
                if (IS_NPC(mst))
                    return;
            }
        }

        return;
    }

    if (!str_cmp(arg1, "credits")
        || !str_cmp(arg1, "credit")
        || !str_cmp(arg1, "money"))
    {
        int amount;

        if (ch->subclass == SUBCLASS_SNEAK)  //Subclass bonus -- 50%!
            amount = (int)(victim->gold * number_range(5, 15) / 100);
        else
            amount = (int)(victim->gold * number_range(1, 10) / 100);

        if (amount > victim->gold)
            amount = victim->gold;
        if (amount <= 0)
        {
            send_to_char("You couldn't get any credits.\n\r", ch);
            learn_from_failure(ch, gsn_steal);
            return;
        }

        ch->gold += amount;
        victim->gold -= amount;
        ch_printf(ch, "Aha!  You got %d credits.\n\r", amount);
        learn_from_success(ch, gsn_steal);
        
        if (IS_NPC(victim))
        {
            xp = UMIN (amount * 10,
                    (exp_level(ch->skill_level[SMUGGLING_ABILITY] + 1) -
                        exp_level(ch->skill_level[SMUGGLING_ABILITY])) / 35);
            xp = UMIN(xp, xp_compute(ch, victim));
            gain_exp(ch, xp, SMUGGLING_ABILITY);            
            ch_printf(ch, "&WYou gain %d smuggling experience!\n\r", xp);
        }
        return;
    }

    if ((obj = get_obj_carry(victim, arg1)) == NULL)
    {
        if (victim->position <= POS_SLEEPING)
        {
            if ((obj = get_obj_wear(victim, arg1)) != NULL)
            {
                if ((obj_next = get_eq_char(victim, obj->wear_loc)) != obj)
                {
                    ch_printf(ch, "They are wearing %s on top of %s.\n\r", obj_next->short_descr, obj->short_descr);
                    send_to_char("You'll have to steal that first.\n\r", ch);
                    learn_from_failure(ch, gsn_steal);
                    return;
                }
                else
                {
                    if (!can_drop_obj(ch, obj)
                        || IS_OBJ_STAT(obj, ITEM_PROTOTYPE)
                        || IS_OBJ_STAT(obj, ITEM_NOREMOVE))
                    {
                        send_to_char("You can't manage to pry it away.\n\r", ch);
                        learn_from_failure(ch, gsn_steal);
                        return;
                    }
                    ch_printf(ch, "You carefully unequip %s.\n\r", obj->short_descr);
                    learn_from_success(ch, gsn_steal);
                    separate_obj(obj);
                    unequip_char(victim, obj);
                    return;
                }
            }
        }
        send_to_char("You can't seem to find it.\n\r", ch);
        learn_from_failure(ch, gsn_steal);
        return;
    }

    if (!can_drop_obj(ch, obj)
        || IS_OBJ_STAT(obj, ITEM_PROTOTYPE))
    {
        send_to_char("You can't manage to pry it away.\n\r", ch);
        learn_from_failure(ch, gsn_steal);
        return;
    }

    if (ch->carry_number + (get_obj_number(obj) / obj->count) > can_carry_n(ch))
    {
        send_to_char("You have your hands full.\n\r", ch);
        learn_from_failure(ch, gsn_steal);
        return;
    }

    if (ch->carry_weight + (get_obj_weight(obj) / obj->count) > can_carry_w(ch))
    {
        send_to_char("You can't carry that much weight.\n\r", ch);
        learn_from_failure(ch, gsn_steal);
        return;
    }
    /* Maintain shop code
     * -Lajos
     */
    if (IS_OBJ_STAT(obj, ITEM_INVENTORY) && IS_NPC(victim) && !IS_AFFECTED(victim, AFF_CHARM) && victim->pIndexData->pShop != NULL)
    {
        stolen_obj = create_object(obj->pIndexData, obj->level);
        lower_economy(ch->in_room->area, obj->cost * 3);
        obj = stolen_obj;
        if (!IS_AFFECTED(victim, AFF_SIT_AWARE)) /* Raise awareness for a bit */
        {
            af.type = gsn_aware;
            af.location = APPLY_AC;
            af.modifier = -10;
            af.duration = URANGE(100, victim->top_level * 5 + 30, 300);
            af.bitvector = AFF_SIT_AWARE;
            affect_to_char(victim, &af);
        }
        act(AT_ACTION, "$n starts looking around suspiciously.", victim, NULL, NULL, TO_ROOM);
    }
    else
    {
        separate_obj(obj);
        obj_from_char(obj);
    }
    if (!IS_OBJ_STAT(obj, ITEM_STOLEN))
        SET_BIT(obj->extra_flags, ITEM_STOLEN);
    obj->stolen_from = victim;
    obj_to_char(obj, ch);

    send_to_char("Ok.\n\r", ch);
    learn_from_success(ch, gsn_steal);
    return;
}

void do_backstab( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   CHAR_DATA *victim;
   OBJ_DATA *obj;
   int percent;

   if (!IS_NPC(ch))
   {
       if (IS_SET(ch->pcdata->flags, PCFLAG_BOUND))
       {
           send_to_char("How do you intend to do that when bound?\n\r", ch);
           return;
       }
   }


   if( IS_NPC( ch ) && IS_AFFECTED( ch, AFF_CHARM ) )
   {
      send_to_char( "You can't do that right now.\r\n", ch );
      return;
   }

   one_argument( argument, arg );

   if( ch->mount )
   {
      send_to_char( "You can't get close enough while mounted.\r\n", ch );
      return;
   }

   if( arg[0] == '\0' )
   {
      send_to_char( "Backstab whom?\r\n", ch );
      return;
   }

   if( ( victim = get_char_room( ch, arg ) ) == NULL )
   {
      send_to_char( "They aren't here.\r\n", ch );
      return;
   }

   if( victim == ch )
   {
      send_to_char( "How can you sneak up on yourself?\r\n", ch );
      return;
   }

   if( is_safe( ch, victim ) )
      return;

   /*
    * Added stabbing weapon. -Narn 
    */
   if( ( obj = get_eq_char( ch, WEAR_WIELD ) ) == NULL || ( obj->value[3] != WEAPON_VIBRO_BLADE ) )
   {
      send_to_char( "You need to wield a stabbing weapon.\r\n", ch );
      return;
   }

   if( victim->fighting )
   {
      send_to_char( "You can't backstab someone who is in combat.\r\n", ch );
      return;
   }

   /*
    * Can backstab a char even if it's hurt as long as it's sleeping. -Narn 
    */
   if( victim->hit < victim->max_hit && IS_AWAKE( victim ) )
   {
      act( AT_PLAIN, "$N is hurt and suspicious ... you can't sneak up.", ch, NULL, victim, TO_CHAR );
      return;
   }

   percent = number_percent(  ) - ( get_curr_lck( ch ) - 14 ) + ( get_curr_lck( victim ) - 13 );

   WAIT_STATE( ch, skill_table[gsn_backstab]->beats );
   if( !IS_AWAKE( victim ) || IS_NPC( ch ) || percent < ch->pcdata->learned[gsn_backstab] )
   {
      learn_from_success( ch, gsn_backstab );
      global_retcode = multi_hit( ch, victim, gsn_backstab );

   }
   else
   {
      learn_from_failure( ch, gsn_backstab );
      global_retcode = damage( ch, victim, 0, gsn_backstab );
   }
}


/* New Research Target, By Arcturus */

void do_researchtarget( CHAR_DATA * ch, const char * argument )
{
    AFFECT_DATA af;
    CHAR_DATA* victim;
    //int level, chance;
    int chance;
    if (argument[0] == '\0')
    {
        send_to_char("You need to choose a target.\n\r", ch);
        return;
    }
    if ((victim = get_char_world(ch, argument)) == NULL
        || victim == ch
        || !victim->in_room
        || (IS_NPC(victim)))
    {
        send_to_char("Your target cannot be found.\n\r", ch);
        return;
    }
    if (IS_NPC(victim))
    {
        send_to_char("This skill doesn't work on NPCs.\n\r", ch);
        return;
    }
    chance = IS_NPC(ch) ? ch->top_level
        : (int)(ch->pcdata->learned[gsn_researchtarget]);
    WAIT_STATE(ch, PULSE_VIOLENCE);
    if (number_percent() >= chance)
    {
        ch_printf(ch, "You fail to dig up dirt on %s.\n\r", NAME(victim));
        learn_from_failure(ch, gsn_researchtarget);
        return;
    }

    if (victim->subclass == SUBCLASS_STEALTH_HUNT || victim->subclass == SUBCLASS_SNEAK)
    {
        ch_printf(ch, "It is impossible to dig up dirt on %s.\n\r", NAME(victim));
        learn_from_success(ch, gsn_researchtarget);
        return;
    }


    if (IS_IMMORTAL(victim) && (victim->top_level > ch->top_level))
    {
        af.type = gsn_researchtarget;
        af.location = APPLY_HITROLL;
        af.modifier = -666;
        af.duration = 25; //was 32000, changed lower
        af.bitvector = AFF_BLIND;
        affect_to_char(ch, &af);
        set_char_color(AT_MAGIC, victim);
        send_to_char("You are blinded by your target's immortal aura!\n\r", ch);
        return;
    }
    ch_printf(ch, "&wYour research has concluded the following information:\n\r");
    //ch_printf(ch, "&wTitle: %0.74s&w\n\r", victim->pcdata->title);
    ch_printf(ch, "&wTitle: %s&w\n\r", victim->pcdata->title);
    ch_printf(ch, "&wRace: %s Class: %s Subclass: %s\n\r", npc_race[victim->race], ability_name[victim->main_ability], subclasses[victim->subclass]);
    if (victim->pcdata->clan)
        ch_printf(ch, "Affiliation: %s\n\r", victim->pcdata->clan_name);
    ch_printf(ch, "Hitpoints: %5.5d/%5.5d Movement: %5.5d/%5.5d\n\r", victim->hit, victim->max_hit, victim->move, victim->max_move);
    ch_printf(ch, "Armor: %4.4d Hitroll: %5.5d Damroll: %5.5d Alignment: %4.4d", GET_AC(victim), GET_HITROLL(victim),
        GET_DAMROLL(victim), victim->alignment);
    learn_from_success(ch, gsn_researchtarget);
    return;
}


void do_rescue( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   CHAR_DATA *victim;
   CHAR_DATA *fch;
   int percent;

   if (!IS_NPC(ch))
   {
       if (IS_SET(ch->pcdata->flags, PCFLAG_BOUND))
       {
           send_to_char("How do you intend to do that when bound?\n\r", ch);
           return;
       }
   }


   if( IS_NPC( ch ) && IS_AFFECTED( ch, AFF_CHARM ) )
   {
      send_to_char( "You can't concentrate enough for that.\r\n", ch );
      return;
   }

   one_argument( argument, arg );
   if( arg[0] == '\0' )
   {
      send_to_char( "Rescue whom?\r\n", ch );
      return;
   }

   if( ( victim = get_char_room( ch, arg ) ) == NULL )
   {
      send_to_char( "They aren't here.\r\n", ch );
      return;
   }

   if( victim == ch )
   {
      send_to_char( "You try and rescue yourself, but fail miserably.\r\n", ch );
      return;
   }

   if( ch->mount )
   {
      send_to_char( "You can't do that while mounted.\r\n", ch );
      return;
   }

   if( !IS_NPC( ch ) && IS_NPC( victim ) )
   {
      send_to_char( "Doesn't need your help!\r\n", ch );
      return;
   }

   if( ( fch = who_fighting( victim ) ) == NULL )
   {
      send_to_char( "They are not fighting right now.\r\n", ch );
      return;
   }

   if( who_fighting( victim ) == ch )
   {
      send_to_char( "One would imagine THEY don't need your help.\r\n", ch );
      return;
   }

   ch->alignment = ch->alignment + 5;
   ch->alignment = URANGE( -1000, ch->alignment, 1000 );

   percent = number_percent(  ) - ( get_curr_lck( ch ) - 14 ) - ( get_curr_lck( victim ) - 16 );

   WAIT_STATE( ch, skill_table[gsn_rescue]->beats );
   if( !IS_NPC( ch ) && percent > ch->pcdata->learned[gsn_rescue] )
   {
      send_to_char( "You fail the rescue.\r\n", ch );
      act( AT_SKILL, "$n tries to rescue you!", ch, NULL, victim, TO_VICT );
      act( AT_SKILL, "$n tries to rescue $N!", ch, NULL, victim, TO_NOTVICT );
      learn_from_failure( ch, gsn_rescue );
      return;
   }

   act( AT_SKILL, "You rescue $N!", ch, NULL, victim, TO_CHAR );
   act( AT_SKILL, "$n rescues you!", ch, NULL, victim, TO_VICT );
   act( AT_SKILL, "$n moves in front of $N!", ch, NULL, victim, TO_NOTVICT );

   ch->alignment = ch->alignment + 50;
   ch->alignment = URANGE( -1000, ch->alignment, 1000 );

   learn_from_success( ch, gsn_rescue );
   stop_fighting( fch, FALSE );
   stop_fighting( victim, FALSE );
   if( ch->fighting )
      stop_fighting( ch, FALSE );

   /*
    * check_killer( ch, fch ); 
    */
   set_fighting( ch, fch );
   set_fighting( fch, ch );
}

void do_cheap( CHAR_DATA * ch, const char *argument )
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;
    //OBJ_DATA *obj;
    int percent;

    if (!IS_NPC(ch))
    {
        if (IS_SET(ch->pcdata->flags, PCFLAG_BOUND))
        {
            send_to_char("Not while your bound.\n\r", ch);
            return;
        }
    }

    if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM))
    {
        send_to_char("You can't do that right now.\n\r", ch);
        return;
    }

    one_argument(argument, arg);

    if (ch->mount)
    {
        send_to_char("You can't get close enough while mounted.\n\r", ch);
        return;
    }

    if (arg[0] == '\0')
    {
        send_to_char("Cheapattack whom?\n\r", ch);
        return;
    }

    if ((victim = get_char_room(ch, arg)) == NULL)
    {
        send_to_char("They aren't here.\n\r", ch);
        return;
    }

    if (victim == ch)
    {
        send_to_char("How can you sneak up on yourself?\n Strange you are\n\r", ch);
        return;
    }

    if (is_safe(ch, victim))
        return;

    /*Was added to make a little funny quote*/
/*
    if ( ( obj = get_eq_char( ch, WEAR_WIELD ) ) == NULL
    ||    ( obj->value[3] != WEAPON_VIBRO_BLADE ) )
    {
       send_to_char( "You need to wield a stabbing weapon.\n\r", ch );
       send_to_char( "Your companion is trying to backstab you with his hands...weird right?\n\r", victim );
       return;
    }
*/

    if (victim->fighting)
    {
        send_to_char("You can't cheapattack someone who is in combat.\n\r", ch);
        return;
    }

    /*  Can cheapattack even if it's hurt as long as it's sleeping.*/
    /*
    if (victim->hit < victim->max_hit && IS_AWAKE(victim))
    {
        act(AT_PLAIN, "$N is hurt and suspicious ... you can't sneak up.", ch, NULL, victim, TO_CHAR);
        return;
    }
    */
    if (!(!IS_AFFECTED(victim, AFF_SIT_AWARE) || !IS_AWAKE(victim)))
    {
        if (number_percent() < 50)
        {
            send_to_char("Your opponent senses you approach and sidesteps your blow!\n\r", ch);
            learn_from_failure(ch, gsn_cheap);
            global_retcode = damage(ch, victim, 0, gsn_cheap);
            return;
        }
    }

    percent = number_percent() - (get_curr_lck(ch) - 14)
        + (get_curr_lck(victim) - 13);

    WAIT_STATE(ch, skill_table[gsn_cheap]->beats);
    if (!IS_AWAKE(victim)
        || IS_NPC(ch)
        || percent < ch->pcdata->learned[gsn_cheap])
    {
        learn_from_success(ch, gsn_cheap);
        global_retcode = multi_hit(ch, victim, gsn_cheap);
    }
    else
    {
        learn_from_failure(ch, gsn_cheap);
        global_retcode = damage(ch, victim, 0, gsn_cheap);
    }
    return;
}

void do_kick( CHAR_DATA * ch, const char *argument )
{
   CHAR_DATA *victim;

   if( IS_NPC( ch ) && IS_AFFECTED( ch, AFF_CHARM ) )
   {
      send_to_char( "You can't concentrate enough for that.\r\n", ch );
      return;
   }

   if( ( victim = who_fighting( ch ) ) == NULL )
   {
      send_to_char( "You aren't fighting anyone.\r\n", ch );
      return;
   }

   WAIT_STATE( ch, skill_table[gsn_kick]->beats );
   if( IS_NPC( ch ) || number_percent(  ) < ch->pcdata->learned[gsn_kick] )
   {
      learn_from_success( ch, gsn_kick );
      global_retcode = damage( ch, victim, number_range( 1, ch->skill_level[COMBAT_ABILITY] ), gsn_kick );
   }
   else
   {
      learn_from_failure( ch, gsn_kick );
      global_retcode = damage( ch, victim, 0, gsn_kick );
   }
}

void do_punch( CHAR_DATA * ch, const char *argument )
{
   CHAR_DATA *victim;

   if (!IS_NPC(ch))
   {
       if (IS_SET(ch->pcdata->flags, PCFLAG_BOUND))
       {
           send_to_char("How do you intend to do that when bound?\n\r", ch);
           return;
       }
   }


   if( IS_NPC( ch ) && IS_AFFECTED( ch, AFF_CHARM ) )
   {
      send_to_char( "You can't concentrate enough for that.\r\n", ch );
      return;
   }

   if( !IS_NPC( ch ) && ch->pcdata->learned[gsn_punch] <= 0 )
   {
      send_to_char( "Your mind races as you realize you have no idea how to do that.\r\n", ch );
      return;
   }

   if( ( victim = who_fighting( ch ) ) == NULL )
   {
      send_to_char( "You aren't fighting anyone.\r\n", ch );
      return;
   }

   WAIT_STATE( ch, skill_table[gsn_punch]->beats );
   if( IS_NPC( ch ) || number_percent(  ) < ch->pcdata->learned[gsn_punch] )
   {
      learn_from_success( ch, gsn_punch );
      global_retcode = damage( ch, victim, number_range( 1, ch->skill_level[COMBAT_ABILITY] ), gsn_punch );
   }
   else
   {
      learn_from_failure( ch, gsn_punch );
      global_retcode = damage( ch, victim, 0, gsn_punch );
   }
}

void do_bite( CHAR_DATA * ch, const char *argument )
{
}

void do_claw( CHAR_DATA * ch, const char *argument )
{
}

void do_sting( CHAR_DATA * ch, const char *argument )
{
}

void do_tail( CHAR_DATA * ch, const char *argument )
{
}

void do_bash( CHAR_DATA * ch, const char *argument )
{
   CHAR_DATA *victim;
   int schance;

   if( IS_NPC( ch ) && IS_AFFECTED( ch, AFF_CHARM ) )
   {
      send_to_char( "You can't concentrate enough for that.\r\n", ch );
      return;
   }

   if( !IS_NPC( ch ) && ch->pcdata->learned[gsn_bash] <= 0 )
   {
      send_to_char( "Your mind races as you realize you have no idea how to do that.\r\n", ch );
      return;
   }

   if( ( victim = who_fighting( ch ) ) == NULL )
   {
      send_to_char( "You aren't fighting anyone.\r\n", ch );
      return;
   }

   schance = ( ( ( get_curr_dex( victim ) + get_curr_str( victim ) )
                 - ( get_curr_dex( ch ) + get_curr_str( ch ) ) ) * 10 ) + 10;
   if( !IS_NPC( ch ) && !IS_NPC( victim ) )
      schance += 25;
   if( victim->fighting && victim->fighting->who != ch )
      schance += 19;
   WAIT_STATE( ch, skill_table[gsn_bash]->beats );
   if( IS_NPC( ch ) || ( number_percent(  ) + schance ) < ch->pcdata->learned[gsn_bash] )
   {
      learn_from_success( ch, gsn_bash );
      /*
       * do not change anything here!  -Thoric 
       */
      WAIT_STATE( ch, 2 * PULSE_VIOLENCE );
      WAIT_STATE( victim, 2 * PULSE_VIOLENCE );
      victim->position = POS_SITTING;
      global_retcode = damage( ch, victim, number_range( 1, ch->skill_level[COMBAT_ABILITY] ), gsn_bash );
   }
   else
   {
      WAIT_STATE( ch, 2 * PULSE_VIOLENCE );
      learn_from_failure( ch, gsn_bash );
      global_retcode = damage( ch, victim, 0, gsn_bash );
   }
}

void do_stun( CHAR_DATA * ch, const char *argument )
{
   CHAR_DATA *victim;
   AFFECT_DATA af;
   int schance;
   bool fail;

   if (!IS_NPC(ch))
   {
       if (IS_SET(ch->pcdata->flags, PCFLAG_BOUND))
       {
           send_to_char("How do you intend to do that when bound?\n\r", ch);
           return;
       }
   }


   if( IS_NPC( ch ) && IS_AFFECTED( ch, AFF_CHARM ) )
   {
      send_to_char( "You can't concentrate enough for that.\r\n", ch );
      return;
   }

   if( !IS_NPC( ch ) && ch->pcdata->learned[gsn_stun] <= 0 )
   {
      send_to_char( "Your mind races as you realize you have no idea how to do that.\r\n", ch );
      return;
   }

   if( ( victim = who_fighting( ch ) ) == NULL )
   {
      send_to_char( "You aren't fighting anyone.\r\n", ch );
      return;
   }

   if( ch->move < 16 )
   {
      set_char_color( AT_SKILL, ch );
      send_to_char( "You are far too tired to do that.\r\n", ch );
      return;  /* missing return fixed March 11/96 */
   }

   WAIT_STATE( ch, skill_table[gsn_stun]->beats );
   fail = FALSE;
   schance = ris_save( victim, ch->skill_level[COMBAT_ABILITY], RIS_PARALYSIS );
   if( schance == 1000 )
      fail = TRUE;
   else
      fail = saves_para_petri( schance, victim );

   schance = ( ( ( get_curr_dex( victim ) + get_curr_str( victim ) )
                 - ( get_curr_dex( ch ) + get_curr_str( ch ) ) ) * 10 ) + 10;
   /*
    * harder for player to stun another player 
    */
   if( !IS_NPC( ch ) && !IS_NPC( victim ) )
      schance += sysdata.stun_plr_vs_plr;
   else
      schance += sysdata.stun_regular;
   if( !fail && ( IS_NPC( ch ) || ( number_percent(  ) + schance ) < ch->pcdata->learned[gsn_stun] ) )
   {
      learn_from_success( ch, gsn_stun );
      /*
       * DO *NOT* CHANGE!    -Thoric    
       */
      ch->move -= 15;
      WAIT_STATE( ch, 2 * PULSE_VIOLENCE );
      WAIT_STATE( victim, PULSE_VIOLENCE );
      act( AT_SKILL, "$N smashes into you, leaving you stunned!", victim, NULL, ch, TO_CHAR );
      act( AT_SKILL, "You smash into $N, leaving $M stunned!", ch, NULL, victim, TO_CHAR );
      act( AT_SKILL, "$n smashes into $N, leaving $M stunned!", ch, NULL, victim, TO_NOTVICT );
      if( !IS_AFFECTED( victim, AFF_PARALYSIS ) )
      {
         af.type = gsn_stun;
         af.location = APPLY_AC;
         af.modifier = 20;
         af.duration = 3;
         af.bitvector = AFF_PARALYSIS;
         affect_to_char( victim, &af );
         update_pos( victim );
      }
   }
   else
   {
      WAIT_STATE( ch, 2 * PULSE_VIOLENCE );
      ch->move -= 5;
      learn_from_failure( ch, gsn_stun );
      act( AT_SKILL, "$N charges at you screaming, but you dodge out of the way.", victim, NULL, ch, TO_CHAR );
      act( AT_SKILL, "Your attempt to stun $N leaves you racing past $E as $e laughs.", ch, NULL, victim, TO_CHAR );
      act( AT_SKILL, "$n charges screaming at $N, but keeps going right on past.", ch, NULL, victim, TO_NOTVICT );
   }
}

void do_feed( CHAR_DATA * ch, const char *argument )
{
   send_to_char( "It is not of your nature to feed on living creatures.\r\n", ch );
}

/*
 * Disarm a creature.
 * Caller must check for successful attack.
 * Check for loyalty flag (weapon disarms to inventory) for pkillers -Blodkai
 */
void disarm( CHAR_DATA * ch, CHAR_DATA * victim )
{
   OBJ_DATA *obj, *tmpobj;

   if (!IS_NPC(ch))
   {
       if (IS_SET(ch->pcdata->flags, PCFLAG_BOUND))
       {
           send_to_char("How do you intend to do that when bound?\n\r", ch);
           return;
       }
   }

   if( ( obj = get_eq_char( victim, WEAR_WIELD ) ) == NULL )
      return;

   if( ( tmpobj = get_eq_char( victim, WEAR_DUAL_WIELD ) ) != NULL && number_bits( 1 ) == 0 )
      obj = tmpobj;

   if( get_eq_char( ch, WEAR_WIELD ) == NULL && number_bits( 1 ) == 0 )
   {
      learn_from_failure( ch, gsn_disarm );
      return;
   }

   if( IS_NPC( ch ) && !can_see_obj( ch, obj ) && number_bits( 1 ) == 0 )
   {
      learn_from_failure( ch, gsn_disarm );
      return;
   }

   if( check_grip( ch, victim ) )
   {
      learn_from_failure( ch, gsn_disarm );
      return;
   }

   act( AT_SKILL, "$n DISARMS you!", ch, NULL, victim, TO_VICT );
   act( AT_SKILL, "You disarm $N!", ch, NULL, victim, TO_CHAR );
   act( AT_SKILL, "$n disarms $N!", ch, NULL, victim, TO_NOTVICT );
   learn_from_success( ch, gsn_disarm );

   if( obj == get_eq_char( victim, WEAR_WIELD ) && ( tmpobj = get_eq_char( victim, WEAR_DUAL_WIELD ) ) != NULL )
      tmpobj->wear_loc = WEAR_WIELD;

   obj_from_char( obj );
   obj_to_room( obj, victim->in_room );
}

void do_disarm( CHAR_DATA * ch, const char *argument )
{
   CHAR_DATA *victim;
   OBJ_DATA *obj;
   int percent;

   if (!IS_NPC(ch))
   {
       if (IS_SET(ch->pcdata->flags, PCFLAG_BOUND))
       {
           send_to_char("How do you intend to do that when bound?\n\r", ch);
           return;
       }
   }

   if( IS_NPC( ch ) && IS_AFFECTED( ch, AFF_CHARM ) )
   {
      send_to_char( "You can't concentrate enough for that.\r\n", ch );
      return;
   }

   if( !IS_NPC( ch ) && ch->pcdata->learned[gsn_disarm] <= 0 )
   {
      send_to_char( "You don't know how to disarm opponents.\r\n", ch );
      return;
   }

   if( get_eq_char( ch, WEAR_WIELD ) == NULL )
   {
      send_to_char( "You must wield a weapon to disarm.\r\n", ch );
      return;
   }

   if( ( victim = who_fighting( ch ) ) == NULL )
   {
      send_to_char( "You aren't fighting anyone.\r\n", ch );
      return;
   }

   if( ( obj = get_eq_char( victim, WEAR_WIELD ) ) == NULL )
   {
      send_to_char( "Your opponent is not wielding a weapon.\r\n", ch );
      return;
   }

   WAIT_STATE( ch, skill_table[gsn_disarm]->beats );
   percent = number_percent(  ) + victim->skill_level[COMBAT_ABILITY] - ch->skill_level[COMBAT_ABILITY]
      - ( get_curr_lck( ch ) - 15 ) + ( get_curr_lck( victim ) - 15 );
   if( !can_see_obj( ch, obj ) )
      percent += 10;
   if( IS_NPC( ch ) || percent < ch->pcdata->learned[gsn_disarm] * 2 / 3 )
      disarm( ch, victim );
   else
   {
      send_to_char( "You failed.\r\n", ch );
      learn_from_failure( ch, gsn_disarm );
   }
}

/*
 * Trip a creature.
 * Caller must check for successful attack.
 */
void trip( CHAR_DATA * ch, CHAR_DATA * victim )
{
   if( IS_AFFECTED( victim, AFF_FLYING ) || IS_AFFECTED( victim, AFF_FLOATING ) )
      return;
   if( victim->mount )
   {
      if( IS_AFFECTED( victim->mount, AFF_FLYING ) || IS_AFFECTED( victim->mount, AFF_FLOATING ) )
         return;
      act( AT_SKILL, "$n trips your mount and you fall off!", ch, NULL, victim, TO_VICT );
      act( AT_SKILL, "You trip $N's mount and $N falls off!", ch, NULL, victim, TO_CHAR );
      act( AT_SKILL, "$n trips $N's mount and $N falls off!", ch, NULL, victim, TO_NOTVICT );
      REMOVE_BIT( victim->mount->act, ACT_MOUNTED );
      victim->mount = NULL;
      WAIT_STATE( ch, 2 * PULSE_VIOLENCE );
      WAIT_STATE( victim, 2 * PULSE_VIOLENCE );
      victim->position = POS_RESTING;
      return;
   }
   if( victim->wait == 0 )
   {
      act( AT_SKILL, "$n trips you and you go down!", ch, NULL, victim, TO_VICT );
      act( AT_SKILL, "You trip $N and $N goes down!", ch, NULL, victim, TO_CHAR );
      act( AT_SKILL, "$n trips $N and $N goes down!", ch, NULL, victim, TO_NOTVICT );

      WAIT_STATE( ch, 2 * PULSE_VIOLENCE );
      WAIT_STATE( victim, 2 * PULSE_VIOLENCE );
      victim->position = POS_RESTING;
   }
}

void do_pick( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   CHAR_DATA *gch;
   OBJ_DATA *obj;
   EXIT_DATA *pexit;
   SHIP_DATA *ship;

   if (!IS_NPC(ch))
   {
       if (IS_SET(ch->pcdata->flags, PCFLAG_BOUND))
       {
           send_to_char("How do you intend to do that when bound?\n\r", ch);
           return;
       }
   }

   if( IS_NPC( ch ) && IS_AFFECTED( ch, AFF_CHARM ) )
   {
      send_to_char( "You can't concentrate enough for that.\r\n", ch );
      return;
   }

   one_argument( argument, arg );

   if( arg[0] == '\0' )
   {
      send_to_char( "Pick what?\r\n", ch );
      return;
   }

   if( ms_find_obj( ch ) )
      return;

   if( ch->mount )
   {
      send_to_char( "You can't do that while mounted.\r\n", ch );
      return;
   }

   WAIT_STATE( ch, skill_table[gsn_pick_lock]->beats );

   /*
    * look for guards 
    */
   for( gch = ch->in_room->first_person; gch; gch = gch->next_in_room )
   {
      if( IS_NPC( gch ) && IS_AWAKE( gch ) && ch->skill_level[SMUGGLING_ABILITY] < gch->top_level )
      {
         act( AT_PLAIN, "$N is standing too close to the lock.", ch, NULL, gch, TO_CHAR );
         return;
      }
   }


   if( ( pexit = find_door( ch, arg, TRUE ) ) != NULL )
   {
      /*
       * 'pick door' 
       */
      /*
       * ROOM_INDEX_DATA *to_room; 
       *//*
       * Unused 
       */
      EXIT_DATA *pexit_rev;

      if( !IS_SET( pexit->exit_info, EX_CLOSED ) )
      {
         send_to_char( "It's not closed.\r\n", ch );
         return;
      }
      if( pexit->key < 0 )
      {
         send_to_char( "It can't be picked.\r\n", ch );
         return;
      }
      if( !IS_SET( pexit->exit_info, EX_LOCKED ) )
      {
         send_to_char( "It's already unlocked.\r\n", ch );
         return;
      }
      if( IS_SET( pexit->exit_info, EX_PICKPROOF ) )
      {
         send_to_char( "You failed.\r\n", ch );
         learn_from_failure( ch, gsn_pick_lock );
         check_room_for_traps( ch, TRAP_PICK | trap_door[pexit->vdir] );
         return;
      }

      if( !IS_NPC( ch ) && number_percent(  ) > ch->pcdata->learned[gsn_pick_lock] )
      {
         send_to_char( "You failed.\r\n", ch );
         learn_from_failure( ch, gsn_pick_lock );
         return;
      }

      REMOVE_BIT( pexit->exit_info, EX_LOCKED );
      send_to_char( "*Click*\r\n", ch );
      act( AT_ACTION, "$n picks the $d.", ch, NULL, pexit->keyword, TO_ROOM );
      learn_from_success( ch, gsn_pick_lock );
      /*
       * pick the other side 
       */
      if( ( pexit_rev = pexit->rexit ) != NULL && pexit_rev->to_room == ch->in_room )
      {
         REMOVE_BIT( pexit_rev->exit_info, EX_LOCKED );
      }
      check_room_for_traps( ch, TRAP_PICK | trap_door[pexit->vdir] );
      return;
   }

   if( ( obj = get_obj_here( ch, arg ) ) != NULL )
   {
      if( obj->item_type != ITEM_CONTAINER )
      {
         send_to_char( "You can't pick that.\r\n", ch );
         return;
      }
      if( !IS_SET( obj->value[1], CONT_CLOSED ) )
      {
         send_to_char( "It's not closed.\r\n", ch );
         return;
      }
      if( obj->value[2] < 0 )
      {
         send_to_char( "It can't be unlocked.\r\n", ch );
         return;
      }
      if( !IS_SET( obj->value[1], CONT_LOCKED ) )
      {
         send_to_char( "It's already unlocked.\r\n", ch );
         return;
      }
      if( IS_SET( obj->value[1], CONT_PICKPROOF ) )
      {
         send_to_char( "You failed.\r\n", ch );
         learn_from_failure( ch, gsn_pick_lock );
         check_for_trap( ch, obj, TRAP_PICK );
         return;
      }

      if( !IS_NPC( ch ) && number_percent(  ) > ch->pcdata->learned[gsn_pick_lock] )
      {
         send_to_char( "You failed.\r\n", ch );
         learn_from_failure( ch, gsn_pick_lock );
         return;
      }

      separate_obj( obj );
      REMOVE_BIT( obj->value[1], CONT_LOCKED );
      send_to_char( "*Click*\r\n", ch );
      act( AT_ACTION, "$n picks $p.", ch, obj, NULL, TO_ROOM );
      learn_from_success( ch, gsn_pick_lock );
      check_for_trap( ch, obj, TRAP_PICK );
      return;
   }

   if( ( ship = ship_in_room( ch->in_room, arg ) ) != NULL )
   {
      char buf[MAX_STRING_LENGTH];

      if( check_pilot( ch, ship ) )
      {
         send_to_char( "&RWhat would be the point of that!\r\n", ch );
         return;
      }

      if( ship->shipstate != SHIP_DOCKED && ship->shipstate != SHIP_DISABLED )
      {
         send_to_char( "&RThat ship has already started to launch", ch );
         return;
      }

      WAIT_STATE( ch, skill_table[gsn_pickshiplock]->beats );

      if( IS_NPC( ch ) || !ch->pcdata || number_percent(  ) > ch->pcdata->learned[gsn_pickshiplock] )
      {
         send_to_char( "You failed.\r\n", ch );
         snprintf( buf, MAX_STRING_LENGTH, "[ALARM] %s attempting to pick %s.", ch->name, ship->name );
         echo_to_all( AT_RED, buf, 0 );
         learn_from_failure( ch, gsn_pickshiplock );
         return;
      }

      if( !ship->hatchopen )
      {
         ship->hatchopen = TRUE;
         act( AT_PLAIN, "You pick the lock and open the hatch on $T.", ch, NULL, ship->name, TO_CHAR );
         act( AT_PLAIN, "$n picks open the hatch on $T.", ch, NULL, ship->name, TO_ROOM );
         echo_to_room( AT_YELLOW, get_room_index( ship->entrance ), "The hatch opens from the outside." );
         learn_from_success( ch, gsn_pickshiplock );
      }
      return;
   }

   ch_printf( ch, "You see no %s here.\r\n", arg );
}

void do_sneak( CHAR_DATA * ch, const char *argument )
{
   AFFECT_DATA af;

   if( IS_NPC( ch ) && IS_AFFECTED( ch, AFF_CHARM ) )
   {
      send_to_char( "You can't concentrate enough for that.\r\n", ch );
      return;
   }

   if( ch->mount )
   {
      send_to_char( "You can't do that while mounted.\r\n", ch );
      return;
   }

   send_to_char( "You attempt to move silently.\r\n", ch );
   affect_strip( ch, gsn_sneak );

   if( IS_NPC( ch ) || number_percent(  ) < ch->pcdata->learned[gsn_sneak] )
   {
      af.type = gsn_sneak;
      af.duration = ( int )( ch->skill_level[SMUGGLING_ABILITY] * DUR_CONV );
      af.location = APPLY_NONE;
      af.modifier = 0;
      af.bitvector = AFF_SNEAK;
      affect_to_char( ch, &af );
      learn_from_success( ch, gsn_sneak );
   }
   else
      learn_from_failure( ch, gsn_sneak );
}

void do_trace_comlink(CHAR_DATA * ch, const char *argument)
{
    AFFECT_DATA af;

    if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM))
    {
        send_to_char("You can't concentrate enough for that.\n\r", ch);
        return;
    }

    if (ch->mount)
    {
        send_to_char("You can't do that while mounted.\n\r", ch);
        return;
    }

    send_to_char("You begin messing with some wires on your comlink.\n\r", ch);
    affect_strip(ch, gsn_trace_comlink);

    if (IS_NPC(ch) || number_percent() < ch->pcdata->learned[gsn_trace_comlink])
    {
        af.type = gsn_trace_comlink;
        af.duration = ch->skill_level[HUNTING_ABILITY] * DUR_CONV;
        af.location = APPLY_NONE;
        af.modifier = 0;
        af.bitvector = AFF_TRACING;
        affect_to_char(ch, &af);
        learn_from_success(ch, gsn_trace_comlink);
    }
    else
        learn_from_failure(ch, gsn_trace_comlink);

    return;
}

void do_hide( CHAR_DATA * ch, const char *argument )
{
   if( IS_NPC( ch ) && IS_AFFECTED( ch, AFF_CHARM ) )
   {
      send_to_char( "You can't concentrate enough for that.\r\n", ch );
      return;
   }

   if( ch->mount )
   {
      send_to_char( "You can't do that while mounted.\r\n", ch );
      return;
   }

   send_to_char( "You attempt to hide.\r\n", ch );

   if( IS_AFFECTED( ch, AFF_HIDE ) )
      REMOVE_BIT( ch->affected_by, AFF_HIDE );

   if( IS_NPC( ch ) || number_percent(  ) < ch->pcdata->learned[gsn_hide] )
   {
      SET_BIT( ch->affected_by, AFF_HIDE );
      learn_from_success( ch, gsn_hide );
   }
   else
      learn_from_failure( ch, gsn_hide );
}

/*
 * Contributed by Alander.
 */
void do_visible( CHAR_DATA * ch, const char *argument )
{
   affect_strip( ch, gsn_invis );
   affect_strip( ch, gsn_mass_invis );
   affect_strip( ch, gsn_sneak );
   REMOVE_BIT( ch->affected_by, AFF_HIDE );
   if( ch->race != RACE_DEFEL )  /* Defel has perm invis */
      REMOVE_BIT( ch->affected_by, AFF_INVISIBLE );
   if( ch->race != RACE_NOGHRI ) /* Noghri has perm sneak */
      REMOVE_BIT( ch->affected_by, AFF_SNEAK );
   send_to_char( "Ok.\r\n", ch );
}

void do_recall( CHAR_DATA * ch, const char *argument )
{
   ROOM_INDEX_DATA *location;
   CHAR_DATA *opponent;

   location = NULL;

   location = get_room_index( wherehome( ch ) );

   if( get_trust( ch ) < LEVEL_IMMORTAL )
   {
      AREA_DATA *pArea;

      if( !ch->pcdata || !( pArea = ch->pcdata->area ) )
      {
         send_to_char( "Only builders can recall.\r\n", ch );
         return;
      }

      if( ch->in_room->vnum < pArea->low_r_vnum || ch->in_room->vnum > pArea->hi_r_vnum )
      {
         send_to_char( "You can only recall from your assigned area.\r\n", ch );
         return;
      }
   }

   if( !location )
   {
      send_to_char( "You are completely lost.\r\n", ch );
      return;
   }

   if( ch->in_room == location )
      return;

   if( IS_SET( ch->in_room->room_flags, ROOM_NO_RECALL ) )
   {
      send_to_char( "For some strange reason... nothing happens.\r\n", ch );
      return;
   }

   //if( IS_SET( ch->affected_by, AFF_CURSE ) )
  // {
  //    send_to_char( "You are cursed and cannot recall!\r\n", ch );
  //    return;
  // }

   if( ( opponent = who_fighting( ch ) ) != NULL )
   {

      if( number_bits( 1 ) == 0 || ( !IS_NPC( opponent ) && number_bits( 3 ) > 1 ) )
      {
         WAIT_STATE( ch, 4 );
         ch_printf( ch, "You failed!\r\n" );
         return;
      }

      ch_printf( ch, "You recall from combat!\r\n" );
      stop_fighting( ch, TRUE );
   }

   act( AT_ACTION, "$n disappears in a swirl of the Force.", ch, NULL, NULL, TO_ROOM );
   char_from_room( ch );
   char_to_room( ch, location );
   if( ch->mount )
   {
      char_from_room( ch->mount );
      char_to_room( ch->mount, location );
   }
   act( AT_ACTION, "$n appears in a swirl of the Force.", ch, NULL, NULL, TO_ROOM );
   do_look( ch, "auto" );
}

void do_aid( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   CHAR_DATA *victim;
   int percent;

   if (!IS_NPC(ch))
   {
       if (IS_SET(ch->pcdata->flags, PCFLAG_BOUND))
       {
           send_to_char("How do you intend to do that when bound?\n\r", ch);
           return;
       }
   }

   if( IS_NPC( ch ) && IS_AFFECTED( ch, AFF_CHARM ) )
   {
      send_to_char( "You can't concentrate enough for that.\r\n", ch );
      return;
   }

   one_argument( argument, arg );
   if( arg[0] == '\0' )
   {
      send_to_char( "Aid whom?\r\n", ch );
      return;
   }

   if( ( victim = get_char_room( ch, arg ) ) == NULL )
   {
      send_to_char( "They aren't here.\r\n", ch );
      return;
   }

   if( ch->mount )
   {
      send_to_char( "You can't do that while mounted.\r\n", ch );
      return;
   }

   if( victim == ch )
   {
      send_to_char( "Aid yourself?\r\n", ch );
      return;
   }

   if( victim->position > POS_STUNNED )
   {
      act( AT_PLAIN, "$N doesn't need your help.", ch, NULL, victim, TO_CHAR );
      return;
   }

   if( victim->hit <= -400 )
   {
      act( AT_PLAIN, "$N's condition is beyond your aiding ability.", ch, NULL, victim, TO_CHAR );
      return;
   }

   ch->alignment = ch->alignment + 20;
   ch->alignment = URANGE( -1000, ch->alignment, 1000 );

   percent = number_percent(  ) - ( get_curr_lck( ch ) - 13 );
   WAIT_STATE( ch, skill_table[gsn_aid]->beats );
   if( !IS_NPC( ch ) && percent > ch->pcdata->learned[gsn_aid] )
   {
      send_to_char( "You fail.\r\n", ch );
      learn_from_failure( ch, gsn_aid );
      return;
   }

   ch->alignment = ch->alignment + 20;
   ch->alignment = URANGE( -1000, ch->alignment, 1000 );

   act( AT_SKILL, "You aid $N!", ch, NULL, victim, TO_CHAR );
   act( AT_SKILL, "$n aids $N!", ch, NULL, victim, TO_NOTVICT );
   learn_from_success( ch, gsn_aid );
   if( victim->hit < 1 )
      victim->hit = 1;

   update_pos( victim );
   act( AT_SKILL, "$n aids you!", ch, NULL, victim, TO_VICT );
}

void do_mount( CHAR_DATA * ch, const char *argument )
{
   CHAR_DATA *victim;

   if (!IS_NPC(ch))
   {
       if (IS_SET(ch->pcdata->flags, PCFLAG_BOUND))
       {
           send_to_char("How do you intend to do that when bound?\n\r", ch);
           return;
       }
   }

   if( !IS_NPC( ch ) && ch->pcdata->learned[gsn_mount] <= 0 )
   {
      send_to_char( "I don't think that would be a good idea...\r\n", ch );
      return;
   }

   if( ch->mount )
   {
      send_to_char( "You're already mounted!\r\n", ch );
      return;
   }

   if( ( victim = get_char_room( ch, argument ) ) == NULL )
   {
      send_to_char( "You can't find that here.\r\n", ch );
      return;
   }

   if( !IS_NPC( victim ) || !IS_SET( victim->act, ACT_MOUNTABLE ) )
   {
      send_to_char( "You can't mount that!\r\n", ch );
      return;
   }

   if( IS_SET( victim->act, ACT_MOUNTED ) )
   {
      send_to_char( "That mount already has a rider.\r\n", ch );
      return;
   }

   if( victim->position < POS_STANDING )
   {
      send_to_char( "Your mount must be standing.\r\n", ch );
      return;
   }

   if( victim->position == POS_FIGHTING || victim->fighting )
   {
      send_to_char( "Your mount is moving around too much.\r\n", ch );
      return;
   }

   WAIT_STATE( ch, skill_table[gsn_mount]->beats );
   if( IS_NPC( ch ) || number_percent(  ) < ch->pcdata->learned[gsn_mount] )
   {
      SET_BIT( victim->act, ACT_MOUNTED );
      ch->mount = victim;
      act( AT_SKILL, "You mount $N.", ch, NULL, victim, TO_CHAR );
      act( AT_SKILL, "$n skillfully mounts $N.", ch, NULL, victim, TO_NOTVICT );
      act( AT_SKILL, "$n mounts you.", ch, NULL, victim, TO_VICT );
      learn_from_success( ch, gsn_mount );
      ch->position = POS_MOUNTED;
   }
   else
   {
      act( AT_SKILL, "You unsuccessfully try to mount $N.", ch, NULL, victim, TO_CHAR );
      act( AT_SKILL, "$n unsuccessfully attempts to mount $N.", ch, NULL, victim, TO_NOTVICT );
      act( AT_SKILL, "$n tries to mount you.", ch, NULL, victim, TO_VICT );
      learn_from_failure( ch, gsn_mount );
   }
}

void do_dismount( CHAR_DATA * ch, const char *argument )
{
   CHAR_DATA *victim;

   if (!IS_NPC(ch))
   {
       if (IS_SET(ch->pcdata->flags, PCFLAG_BOUND))
       {
           send_to_char("How do you intend to do that when bound?\n\r", ch);
           return;
       }
   }

   if( ( victim = ch->mount ) == NULL )
   {
      send_to_char( "You're not mounted.\r\n", ch );
      return;
   }

   WAIT_STATE( ch, skill_table[gsn_mount]->beats );
   if( IS_NPC( ch ) || number_percent(  ) < ch->pcdata->learned[gsn_mount] )
   {
      act( AT_SKILL, "You dismount $N.", ch, NULL, victim, TO_CHAR );
      act( AT_SKILL, "$n skillfully dismounts $N.", ch, NULL, victim, TO_NOTVICT );
      act( AT_SKILL, "$n dismounts you.  Whew!", ch, NULL, victim, TO_VICT );
      REMOVE_BIT( victim->act, ACT_MOUNTED );
      ch->mount = NULL;
      ch->position = POS_STANDING;
      learn_from_success( ch, gsn_mount );
   }
   else
   {
      act( AT_SKILL, "You fall off while dismounting $N.  Ouch!", ch, NULL, victim, TO_CHAR );
      act( AT_SKILL, "$n falls off of $N while dismounting.", ch, NULL, victim, TO_NOTVICT );
      act( AT_SKILL, "$n falls off your back.", ch, NULL, victim, TO_VICT );
      learn_from_failure( ch, gsn_mount );
      REMOVE_BIT( victim->act, ACT_MOUNTED );
      ch->mount = NULL;
      ch->position = POS_SITTING;
      global_retcode = damage( ch, ch, 1, TYPE_UNDEFINED );
   }
}

/**************************************************************************/


int check_reflexes(CHAR_DATA* ch)
{
    int bonus = 0;
    int combat_level;
    int force_level;

    if (IS_NPC(ch))
        return 0;

    if ((ch->pcdata->learned[gsn_combat_reflexes])/*(ch, gsn_combat_reflexes)*/)
    {
        combat_level = ch->skill_level[skill_table[gsn_combat_reflexes]->guild];
        bonus += UMIN(1, ((ch->pcdata->learned[gsn_combat_reflexes]) * combat_level) / 1000);
    }
    if ((ch->pcdata->learned[gsn_force_reflexes])/*(ch, gsn_force_reflexes)*/)
    {
        force_level = ch->skill_level[skill_table[gsn_force_reflexes]->guild];
        bonus += UMIN(1, ((ch->pcdata->learned[gsn_force_reflexes]) * force_level) / 1000);
    }
    return bonus;
}

void improve_reflexes(CHAR_DATA* ch, bool success)
{
    if (IS_NPC(ch))
        return;

    if (success)
    {
        learn_from_success(ch, gsn_combat_reflexes);
        learn_from_success(ch, gsn_force_reflexes);
    }
    else
    {
        learn_from_failure(ch, gsn_combat_reflexes);
        learn_from_failure(ch, gsn_force_reflexes);
    }
}


/*
 * Check for parry.
 */
bool check_parry( CHAR_DATA * ch, CHAR_DATA * victim )
{
   int chances = 0;
   OBJ_DATA *wield;
   // bool can_parry = FALSE;
   int reflexes = 0;
   int skill_cap;
   //bool ranged_parry = FALSE;
   //bool can_parry_ranged = FALSE;


   if( !IS_AWAKE( victim ) )
      return FALSE;

   if( IS_NPC( victim ) && !IS_SET( victim->defenses, DFND_PARRY ) )
      return FALSE;
   if( victim->subclass == SUBCLASS_BLADEMASTER)
   {
       chances += 25;
       skill_cap = 75;
       //can_parry_ranged = TRUE;
   }
   else if (victim->subclass == SUBCLASS_SITH_HUNTER)
   {
       //can_parry_ranged = TRUE;
       skill_cap = 75;
   }
   else
       skill_cap = 50;

   reflexes = check_reflexes(victim);
   if (reflexes > 0)
   //can_parry_ranged = TRUE;
   chances += reflexes;
   skill_cap += reflexes;
   skill_cap = URANGE(50, skill_cap, 75);
   chances = URANGE(10, chances, skill_cap);

   if( IS_NPC( victim ) )
   {
      /*
       * Tuan was here.  :)   *** so was Durga :p *** 
       */
      chances = UMIN( 60, victim->skill_level[COMBAT_ABILITY] );
   }
   else
   {
      if( ( wield = get_eq_char( victim, WEAR_WIELD ) ) == NULL || ( wield->value[3] != WEAPON_LIGHTSABER ) )
      {
         if( ( wield = get_eq_char( victim, WEAR_DUAL_WIELD ) ) == NULL || ( wield->value[3] != WEAPON_LIGHTSABER ) )
            return FALSE;
      }
      chances = ( int )( victim->pcdata->learned[gsn_parry] );
   }

   chances = URANGE( 10, chances, 90 );

   if( number_range( 1, 100 ) > chances )
   {
      learn_from_failure( victim, gsn_parry );
      return FALSE;
   }
   if( !IS_NPC( victim ) && !IS_SET( victim->pcdata->flags, PCFLAG_GAG ) )
       /*SB*/ act( AT_SKILL, "You parry $n's attack.", ch, NULL, victim, TO_VICT );

   if( !IS_NPC( ch ) && !IS_SET( ch->pcdata->flags, PCFLAG_GAG ) )   /* SB */
      act( AT_SKILL, "$N parries your attack.", ch, NULL, victim, TO_CHAR );

   learn_from_success( victim, gsn_parry );
   /*added from SWL code base */
   if (victim->subclass == SUBCLASS_BLADEMASTER)
       victim->move -= (2);
   else
       victim->move -= (10);
   return TRUE;
}

/*
 * Check for dodge.
 */
bool check_dodge( CHAR_DATA * ch, CHAR_DATA * victim )
{
   int chances;

   if( !IS_AWAKE( victim ) )
      return FALSE;

   if( IS_NPC( victim ) && !IS_SET( victim->defenses, DFND_DODGE ) )
      return FALSE;

   if( IS_NPC( victim ) )
      chances = UMIN( 60, victim->top_level );
   else
      chances = ( int )( victim->pcdata->learned[gsn_dodge] / 2 );

   if( number_range( 1, 100 ) > chances )
   {
      learn_from_failure( victim, gsn_dodge );
      return FALSE;
   }

   if( !IS_NPC( victim ) && !IS_SET( victim->pcdata->flags, PCFLAG_GAG ) )
      act( AT_SKILL, "You dodge $n's attack.", ch, NULL, victim, TO_VICT );

   if( !IS_NPC( ch ) && !IS_SET( ch->pcdata->flags, PCFLAG_GAG ) )
      act( AT_SKILL, "$N dodges your attack.", ch, NULL, victim, TO_CHAR );

   learn_from_success( victim, gsn_dodge );
   return TRUE;
}

bool check_duck( CHAR_DATA * ch, CHAR_DATA * victim )
{
    int chances;

    if (!IS_AWAKE(victim))
        return FALSE;

    if (IS_NPC(victim) && !IS_SET(victim->defenses, DFND_DUCK))
        return FALSE;

    //if (!IS_NPC(victim) && victim->pcdata->learned[gsn_duck] < 1)
    //    return FALSE;

    if (IS_NPC(victim))
        chances = UMIN(60, victim->top_level);
    else
        chances = (int)(victim->pcdata->learned[gsn_duck] / 2);

    //chances = URANGE(10, chances, 50);

    //if (number_percent() > chances)
    if( number_range( 1, 100) > chances )
    {
        learn_from_failure(victim, gsn_duck);
        return FALSE;
    }

    if (victim->move <= 100 && !IS_NPC(victim))
    {
        act(AT_SKILL, "You're too tired to duck $n's attack!", ch, NULL, victim, TO_VICT);
        return FALSE;
    }

    if (!IS_NPC(victim) && !IS_SET(victim->pcdata->flags, PCFLAG_GAG))
        act(AT_SKILL, "You duck under $n's attack.", ch, NULL, victim, TO_VICT);

    if (!IS_NPC(ch) && !IS_SET(ch->pcdata->flags, PCFLAG_GAG))
        act(AT_SKILL, "$N ducks under your attack.", ch, NULL, victim, TO_CHAR);

    learn_from_success(victim, gsn_duck);

    victim->move -= (18);

    if (victim->move <= 0)
        victim->move = 0;
    return TRUE;
}

void do_poison_weapon( CHAR_DATA * ch, const char *argument )
{
   OBJ_DATA *obj;
   OBJ_DATA *pobj;
   OBJ_DATA *wobj;
   char arg[MAX_INPUT_LENGTH];
   int percent;

   if (!IS_NPC(ch))
   {
       if (IS_SET(ch->pcdata->flags, PCFLAG_BOUND))
       {
           send_to_char("How do you intend to do that when bound?\n\r", ch);
           return;
       }
   }


   if( !IS_NPC( ch ) && ch->pcdata->learned[gsn_poison_weapon] <= 0 )
   {
      send_to_char( "What do you think you are, a thief?\r\n", ch );
      return;
   }

   one_argument( argument, arg );

   if( arg[0] == '\0' )
   {
      send_to_char( "What are you trying to poison?\r\n", ch );
      return;
   }
   if( ch->fighting )
   {
      send_to_char( "While you're fighting?  Nice try.\r\n", ch );
      return;
   }
   if( ms_find_obj( ch ) )
      return;

   if( !( obj = get_obj_carry( ch, arg ) ) )
   {
      send_to_char( "You do not have that weapon.\r\n", ch );
      return;
   }
   if( obj->item_type != ITEM_WEAPON )
   {
      send_to_char( "That item is not a weapon.\r\n", ch );
      return;
   }
   if( IS_OBJ_STAT( obj, ITEM_POISONED ) )
   {
      send_to_char( "That weapon is already poisoned.\r\n", ch );
      return;
   }
   /*
    * Now we have a valid weapon...check to see if we have the powder. 
    */
   for( pobj = ch->first_carrying; pobj; pobj = pobj->next_content )
   {
      if( pobj->pIndexData->vnum == OBJ_VNUM_BLACK_POWDER )
         break;
   }
   if( !pobj )
   {
      send_to_char( "You do not have the black poison powder.\r\n", ch );
      return;
   }
   /*
    * Okay, we have the powder...do we have water? 
    */
   for( wobj = ch->first_carrying; wobj; wobj = wobj->next_content )
   {
      if( wobj->item_type == ITEM_DRINK_CON && wobj->value[1] > 0 && wobj->value[2] == 0 )
         break;
   }
   if( !wobj )
   {
      send_to_char( "You have no water to mix with the powder.\r\n", ch );
      return;
   }
   /*
    * And does the thief have steady enough hands? 
    */
   if( !IS_NPC( ch ) && ( ch->pcdata->condition[COND_DRUNK] > 0 ) )
   {
      send_to_char( "Your hands aren't steady enough to properly mix the poison.\r\n", ch );
      return;
   }
   WAIT_STATE( ch, skill_table[gsn_poison_weapon]->beats );

   percent = ( number_percent(  ) - get_curr_lck( ch ) - 14 );

   /*
    * Check the skill percentage 
    */
   separate_obj( pobj );
   separate_obj( wobj );
   if( !IS_NPC( ch ) && percent > ch->pcdata->learned[gsn_poison_weapon] )
   {
      set_char_color( AT_RED, ch );
      send_to_char( "You failed and spill some on yourself.  Ouch!\r\n", ch );
      set_char_color( AT_GREY, ch );
      damage( ch, ch, ch->skill_level[HUNTING_ABILITY], gsn_poison_weapon );
      act( AT_RED, "$n spills the poison all over!", ch, NULL, NULL, TO_ROOM );
      extract_obj( pobj );
      extract_obj( wobj );
      learn_from_failure( ch, gsn_poison_weapon );
      return;
   }
   separate_obj( obj );
   /*
    * Well, I'm tired of waiting.  Are you? 
    */
   act( AT_RED, "You mix $p in $P, creating a deadly poison!", ch, pobj, wobj, TO_CHAR );
   act( AT_RED, "$n mixes $p in $P, creating a deadly poison!", ch, pobj, wobj, TO_ROOM );
   act( AT_GREEN, "You pour the poison over $p, which glistens wickedly!", ch, obj, NULL, TO_CHAR );
   act( AT_GREEN, "$n pours the poison over $p, which glistens wickedly!", ch, obj, NULL, TO_ROOM );
   SET_BIT( obj->extra_flags, ITEM_POISONED );
   obj->cost *= ch->skill_level[HUNTING_ABILITY] / 2;
   /*
    * Set an object timer.  Don't want proliferation of poisoned weapons 
    */
   obj->timer = 10 + ch->skill_level[HUNTING_ABILITY];

   if( IS_OBJ_STAT( obj, ITEM_BLESS ) )
      obj->timer *= 2;

   if( IS_OBJ_STAT( obj, ITEM_MAGIC ) )
      obj->timer *= 2;

   /*
    * WHAT?  All of that, just for that one bit?  How lame. ;) 
    */
   act( AT_BLUE, "The remainder of the poison eats through $p.", ch, wobj, NULL, TO_CHAR );
   act( AT_BLUE, "The remainder of the poison eats through $p.", ch, wobj, NULL, TO_ROOM );
   extract_obj( pobj );
   extract_obj( wobj );
   learn_from_success( ch, gsn_poison_weapon );
}

void do_scribe( CHAR_DATA * ch, const char *argument )
{
}

void do_brew( CHAR_DATA * ch, const char *argument )
{
}

bool check_grip( CHAR_DATA * ch, CHAR_DATA * victim )
{
   int schance;

   if( !IS_AWAKE( victim ) )
      return FALSE;

   if( IS_NPC( victim ) && !IS_SET( victim->defenses, DFND_GRIP ) )
      return FALSE;

   if( IS_NPC( victim ) )
      schance = UMIN( 60, 2 * victim->top_level );
   else
      schance = ( int )( victim->pcdata->learned[gsn_grip] / 2 );

   /*
    * Consider luck as a factor 
    */
   schance += ( 2 * ( get_curr_lck( victim ) - 13 ) );

   if( number_percent(  ) >= schance + victim->top_level - ch->top_level )
   {
      learn_from_failure( victim, gsn_grip );
      return FALSE;
   }
   act( AT_SKILL, "You evade $n's attempt to disarm you.", ch, NULL, victim, TO_VICT );
   act( AT_SKILL, "$N holds $S weapon strongly, and is not disarmed.", ch, NULL, victim, TO_CHAR );
   learn_from_success( victim, gsn_grip );
   return TRUE;
}

void do_circle( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   CHAR_DATA *victim;
   OBJ_DATA *obj;
   int percent;

   if (!IS_NPC(ch))
   {
       if (IS_SET(ch->pcdata->flags, PCFLAG_BOUND))
       {
           send_to_char("How do you intend to do that when bound?\n\r", ch);
           return;
       }
   }

   if( IS_NPC( ch ) && IS_AFFECTED( ch, AFF_CHARM ) )
   {
      send_to_char( "You can't concentrate enough for that.\r\n", ch );
      return;
   }

   one_argument( argument, arg );

   if( ch->mount )
   {
      send_to_char( "You can't circle while mounted.\r\n", ch );
      return;
   }

   if( arg[0] == '\0' )
   {
      send_to_char( "Circle around whom?\r\n", ch );
      return;
   }

   if( ( victim = get_char_room( ch, arg ) ) == NULL )
   {
      send_to_char( "They aren't here.\r\n", ch );
      return;
   }

   if( victim == ch )
   {
      send_to_char( "How can you sneak up on yourself?\r\n", ch );
      return;
   }

   if( is_safe( ch, victim ) )
      return;

   if( ( obj = get_eq_char( ch, WEAR_WIELD ) ) == NULL || ( obj->value[3] != 11 && obj->value[3] != 2 ) )
   {
      send_to_char( "You need to wield a piercing or stabbing weapon.\r\n", ch );
      return;
   }

   if( !ch->fighting )
   {
      send_to_char( "You can't circle when you aren't fighting.\r\n", ch );
      return;
   }

   if( !victim->fighting )
   {
      send_to_char( "You can't circle around a person who is not fighting.\r\n", ch );
      return;
   }

   if( victim->num_fighting < 2 )
   {
      act( AT_PLAIN, "You can't circle around them without a distraction.", ch, NULL, victim, TO_CHAR );
      return;
   }

   percent = number_percent(  ) - ( get_curr_lck( ch ) - 16 ) + ( get_curr_lck( victim ) - 13 );

   WAIT_STATE( ch, skill_table[gsn_circle]->beats );
   if( percent < ( IS_NPC( ch ) ? ( ch->skill_level[HUNTING_ABILITY] * 1.5 ) : ch->pcdata->learned[gsn_circle] ) )
   {
      learn_from_success( ch, gsn_circle );
      global_retcode = multi_hit( ch, victim, gsn_circle );

   }
   else
   {
      learn_from_failure( ch, gsn_circle );
      global_retcode = damage( ch, victim, 0, gsn_circle );
   }
}

/* Berserk and HitAll. -- Altrag */
void do_berserk( CHAR_DATA * ch, const char *argument )
{
   short percent;
   AFFECT_DATA af;

   if (!IS_NPC(ch))
   {
       if (IS_SET(ch->pcdata->flags, PCFLAG_BOUND))
       {
           send_to_char("How do you intend to do that when bound?\n\r", ch);
           return;
       }
   }

   if( !ch->fighting )
   {
      send_to_char( "But you aren't fighting!\r\n", ch );
      return;
   }

   if( IS_AFFECTED( ch, AFF_BERSERK ) )
   {
      send_to_char( "Your rage is already at its peak!\r\n", ch );
      return;
   }

   percent = IS_NPC( ch ) ? 80 : ch->pcdata->learned[gsn_berserk];
   WAIT_STATE( ch, skill_table[gsn_berserk]->beats );
   if( !chance( ch, percent ) )
   {
      send_to_char( "You couldn't build up enough rage.\r\n", ch );
      learn_from_failure( ch, gsn_berserk );
      return;
   }
   af.type = gsn_berserk;
   /*
    * Hmmm.. 10-20 combat rounds at level 50.. good enough for most mobs,
    * and if not they can always go berserk again.. shrug.. maybe even
    * too high. -- Altrag 
    */
   af.duration = number_range( ch->top_level / 5, ch->top_level * 2 / 5 );
   /*
    * Hmm.. you get stronger when yer really enraged.. mind over matter
    * type thing.. 
    */
   af.location = APPLY_STR;
   af.modifier = 1;
   af.bitvector = AFF_BERSERK;
   affect_to_char( ch, &af );
   send_to_char( "You start to lose control..\r\n", ch );
   learn_from_success( ch, gsn_berserk );
}

/* External from fight.c */
ch_ret one_hit( CHAR_DATA * ch, CHAR_DATA * victim, int dt );
void do_hitall( CHAR_DATA * ch, const char *argument )
{
   CHAR_DATA *vch;
   CHAR_DATA *vch_next;
   short nvict = 0;
   short nhit = 0;
   short percent;

   if (!IS_NPC(ch))
   {
       if (IS_SET(ch->pcdata->flags, PCFLAG_BOUND))
       {
           send_to_char("How do you intend to do that when bound?\n\r", ch);
           return;
       }
   }

   if( IS_SET( ch->in_room->room_flags, ROOM_SAFE ) )
   {
      send_to_char( "You cannot do that here.\r\n", ch );
      return;
   }

   if( !ch->in_room->first_person )
   {
      send_to_char( "There's no one here!\r\n", ch );
      return;
   }
   percent = IS_NPC( ch ) ? 80 : ch->pcdata->learned[gsn_hitall];
   for( vch = ch->in_room->first_person; vch; vch = vch_next )
   {
      vch_next = vch->next_in_room;
      if( is_same_group( ch, vch ) || !is_legal_kill( ch, vch ) || !can_see( ch, vch ) || is_safe( ch, vch ) )
         continue;
      if( ++nvict > ch->skill_level[COMBAT_ABILITY] / 5 )
         break;
      if( chance( ch, percent ) )
      {
         nhit++;
         global_retcode = one_hit( ch, vch, TYPE_UNDEFINED );
      }
      else
         global_retcode = damage( ch, vch, 0, TYPE_UNDEFINED );
      /*
       * Fireshield, etc. could kill ch too.. :>.. -- Altrag 
       */
      if( global_retcode == rCHAR_DIED || global_retcode == rBOTH_DIED || char_died( ch ) )
         return;
   }
   if( !nvict )
   {
      send_to_char( "There's no one here!\r\n", ch );
      return;
   }
   ch->move = UMAX( 0, ch->move - nvict * 3 + nhit );
   if( nhit )
      learn_from_success( ch, gsn_hitall );
   else
      learn_from_failure( ch, gsn_hitall );
}

bool check_illegal_psteal( CHAR_DATA * ch, CHAR_DATA * victim )
{
   return FALSE;
}

void do_scan( CHAR_DATA * ch, const char *argument )
{
   //OBJ_DATA* wield;
   //OBJ_DATA* obj;
   ROOM_INDEX_DATA *was_in_room;
   ROOM_INDEX_DATA *to_room;
   EXIT_DATA *pexit;
   short dir = -1;
   short dist;
   short max_dist = 5;

   if( IS_AFFECTED( ch, AFF_BLIND ) && ( !IS_AFFECTED( ch, AFF_TRUESIGHT ) ||
    ( !IS_NPC( ch ) && !IS_SET( ch->act, PLR_HOLYLIGHT ) ) ) )
   {
      send_to_char( "Everything looks the same when you're blind...\r\n", ch );
      return;
   }

   if( argument[0] == '\0' )
   {
      send_to_char( "Scan in a direction...\r\n", ch );
      return;
   }

   if( ( dir = get_door( argument ) ) == -1 )
   {
      send_to_char( "Scan in WHAT direction?\r\n", ch );
      return;
   }

   was_in_room = ch->in_room;
   act( AT_GREY, "Scanning $t...", ch, dir_name[dir], NULL, TO_CHAR );
   act( AT_GREY, "$n scans $t.", ch, dir_name[dir], NULL, TO_ROOM );

   if( IS_NPC( ch ) || ( number_percent(  ) > ch->pcdata->learned[gsn_scan] ) )
   {
      act( AT_GREY, "You stop scanning $t as your vision blurs.", ch, dir_name[dir], NULL, TO_CHAR );
      learn_from_failure( ch, gsn_scan );
      return;
   }

   if( ( pexit = get_exit( ch->in_room, dir ) ) == NULL )
   {
      act( AT_GREY, "You can't see $t.", ch, dir_name[dir], NULL, TO_CHAR );
      return;
   }

   if( ch->top_level < 50 )
      max_dist--;
   if( ch->top_level < 20 )
      max_dist--;
   //if (ch->subclass == SUBCLASS_SNIPER && ch->sniperweapon != 0)
   if (ch->subclass == SUBCLASS_SNIPER ) // && ((obj = get_eq_char(ch, WEAR_WIELD)) == NULL || (obj->value[3] != WEAPON_SLUGTHROWER))
       max_dist = 10;
   for ( dist = 1; dist <= max_dist; )
   {
      if( IS_SET( pexit->exit_info, EX_CLOSED ) )
      {
         if( IS_SET( pexit->exit_info, EX_SECRET ) )
            act( AT_GREY, "Your view $t is blocked by a wall.", ch, dir_name[dir], NULL, TO_CHAR );
         else
            act( AT_GREY, "Your view $t is blocked by a door.", ch, dir_name[dir], NULL, TO_CHAR );
         break;
      }

      to_room = NULL;
      if( pexit->distance > 1 )
         to_room = generate_exit( ch->in_room, &pexit );

      if( to_room == NULL )
         to_room = pexit->to_room;

      if( room_is_private( ch, to_room ) && get_trust( ch ) < LEVEL_GREATER )
      {
         act( AT_GREY, "Your view $t is blocked by a private room.", ch, dir_name[dir], NULL, TO_CHAR );
         break;
      }
      char_from_room( ch );
      char_to_room( ch, to_room );
      set_char_color( AT_RMNAME, ch );
      send_to_char( ch->in_room->name, ch );
      send_to_char( "\r\n", ch );
      show_list_to_char( ch->in_room->first_content, ch, FALSE, FALSE );
      show_char_to_char( ch->in_room->first_person, ch );

      switch ( ch->in_room->sector_type )
      {
         default:
            dist++;
            break;
         case SECT_AIR:
            if( number_percent(  ) < 80 )
               dist++;
            break;
         case SECT_INSIDE:
         case SECT_FIELD:
         case SECT_UNDERGROUND:
            dist++;
            break;
         case SECT_FOREST:
         case SECT_CITY:
         case SECT_DESERT:
         case SECT_HILLS:
            dist += 2;
            break;
         case SECT_WATER_SWIM:
         case SECT_WATER_NOSWIM:
            dist += 3;
            break;
         case SECT_MOUNTAIN:
         case SECT_UNDERWATER:
         case SECT_OCEANFLOOR:
            dist += 4;
            break;
      }

      if( dist >= max_dist )
      {
         act( AT_GREY, "Your vision blurs with distance and you see no " "farther $t.", ch, dir_name[dir], NULL, TO_CHAR );
         break;
      }
      if( ( pexit = get_exit( ch->in_room, dir ) ) == NULL )
      {
         act( AT_GREY, "Your view $t is blocked by a wall.", ch, dir_name[dir], NULL, TO_CHAR );
         break;
      }
   }

   char_from_room( ch );
   char_to_room( ch, was_in_room );
   learn_from_success( ch, gsn_scan );
}

void do_slice( CHAR_DATA * ch, const char *argument )
{
    CHAR_DATA* victim;
    char arg1[MAX_INPUT_LENGTH];
    OBJ_DATA* obj;
    //int type, sn;

    if (!IS_NPC(ch))
    {
        if (IS_SET(ch->pcdata->flags, PCFLAG_BOUND))
        {
            send_to_char("How do you intend to do that when bound?\n\r", ch);
            return;
        }
    }

    if (IS_NPC(ch))
        return;

    if (ch->subclass != SUBCLASS_BLADEMASTER)
    {
        send_to_char("Only Swordsmen can execute a proper slice attack!!\n\r", ch);
        return;
    }

    argument = one_argument(argument, arg1);

    if (arg1[0] == '\0')
    {
        send_to_char("Slice whom?\n\r", ch);
        return;
    }

    if ((victim = get_char_room(ch, arg1)) == NULL)
    {
        send_to_char("They aren't here.\n\r", ch);
        return;
    }

    if (victim == ch)
    {
        send_to_char("That's pointless.\n\r", ch);
        return;
    }

    if (IS_SET(ch->in_room->room_flags, ROOM_SAFE))
    {
        set_char_color(AT_MAGIC, ch);
        send_to_char("This isn't a good place to do that.\n\r", ch);
        return;
    }

    if (ch->position <= POS_SLEEPING)
    {
        send_to_char("In your dreams or what?\n\r", ch);
        return;
    }

    if ((obj = get_eq_char(ch, WEAR_WIELD)) == NULL)
    {
        send_to_char("With what weapon?\n\r", ch);
        return;
    }
    else
    {
        if (obj->value[3] != WEAPON_SWORD && obj->value[3] != WEAPON_LIGHTSABER && obj->value[3] != WEAPON_VIBRO_SWORD)
        {
            send_to_char("You can only slice with a lightsaber or a sword.\n\r", ch);
            return;
        }
    }
    global_retcode = one_hit( ch, victim, gsn_slice);
    WAIT_STATE(ch, PULSE_VIOLENCE);
    return;
}


// Blackjack (instant stun) by Parnic
void do_blackjack( CHAR_DATA * ch, const char *argument )
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA* victim;
    OBJ_DATA* obj;
    int chance;
    //int percent;
    AFFECT_DATA af;
    bool fail;

    if (!IS_NPC(ch))
    {
        if (IS_SET(ch->pcdata->flags, PCFLAG_BOUND))
        {
            send_to_char("How do you intend to do that when bound?\n\r", ch);
            return;
        }
    }

    if (IS_NPC(ch) && IS_AFFECTED(ch, AFF_CHARM))
    {
        send_to_char("You can't do that right now.\n\r", ch);
        return;
    }

    one_argument(argument, arg);

    if (ch->mount)
    {
        send_to_char("You can't get close enough while mounted.\n\r", ch);
        return;
    }

    if (arg[0] == '\0')
    {
        send_to_char("Blackjack whom?\n\r", ch);
        return;
    }

    if ((victim = get_char_room(ch, arg)) == NULL)
    {
        send_to_char("They aren't here.\n\r", ch);
        return;
    }

    if (victim == ch)
    {
        send_to_char("Dumbass....\n\r", ch);
    }

    if (is_safe(ch, victim))
        return;

    if ((obj = get_eq_char(ch, WEAR_WIELD)) == NULL
        || (obj->value[3] != WEAPON_BLUDGEON))
    {
        send_to_char("You need to wield a blunt weapon.\n\r", ch);
        return;
    }

    if (victim->fighting)
    {
        send_to_char("You can't blackjack someone who is in combat.\n\r", ch);
        return;
    }

    if (victim->position == POS_STUNNED)
    {
        send_to_char("You can't blackjack someone who's already stunned.\n\r", ch);
        return;
    }

    if (victim->hit < victim->max_hit && IS_AWAKE(victim))
    {
        act(AT_PLAIN, "$N is hurt and suspicious ... you can't sneak up.", ch, NULL, victim, TO_CHAR);
        return;
    }

    if (!IS_AFFECTED(ch, AFF_HIDE))
    {
        send_to_char("You need to be hidden to blackjack someone.\n\r", ch);
        return;
    }


    //situational awareness check added by Raijen, fixed by Arcturus

    if (!(!IS_AFFECTED(victim, AFF_SIT_AWARE) || !IS_AWAKE(victim)))
    {
        if (number_percent() < 60)
        {
            send_to_char("Your opponent senses you approach and sidesteps your blow!\n\r", ch);
            learn_from_failure(ch, gsn_blackjack);
            global_retcode = damage(ch, victim, 0, gsn_blackjack);
            return;
        }
    }
    //percent = (number_percent() - (get_curr_lck(victim) - 20) - (ch->pcdata->learned[gsn_bludgeons] / 2));

    WAIT_STATE(ch, skill_table[gsn_blackjack]->beats);
    /*if (!IS_AWAKE(victim)
        || IS_NPC(ch)
        || percent < UMIN(15, (ch->pcdata->learned[gsn_blackjack] / 2)))
    {
        learn_from_success(ch, gsn_blackjack);
        if (!IS_NPC(victim))
            send_to_char("You have been knocked unconscious!\n\r", victim);
        ch_printf(ch, "You knocked %s unconscious!\n\r", victim->name);
        if (!IS_AFFECTED(victim, AFF_PARALYSIS)) */
    fail = FALSE;

    //chance = URANGE(5, (get_skill_percent(ch, gsn_blackjack) - get_curr_lck(victim)), 95);
    chance = URANGE(5, ((ch->pcdata->learned[gsn_blackjack]) - get_curr_lck(victim)), 95);
    chance = ris_save(victim, chance, RIS_SLEEP);

    if (chance == 1000)
        fail = TRUE;
    else
        fail = check_save(gsn_sleep, chance, ch, victim);

    if (!fail && number_percent() < chance)
    {
        learn_from_success(ch, gsn_blackjack);

        act(AT_ACTION, "You have been knocked unconscious.", ch, NULL, victim, TO_VICT);
        act(AT_ACTION, "You knocked $N unconscious!", ch, NULL, victim, TO_CHAR);

        if (!IS_AFFECTED(victim, AFF_SLEEP))
        {
            af.type = gsn_sleep;
            af.location = APPLY_AC;
            af.modifier = 20;
            af.duration = (ch->pcdata->learned[gsn_blackjack] / 10);
            //af.bitvector = AFF_PARALYSIS;
            af.bitvector = AFF_SLEEP;
            affect_to_char(victim, &af);
            update_pos(victim);
        }
        act(AT_ACTION, "&c$n crumples to the ground.", victim, NULL, NULL, TO_ROOM);
    }
    else
    {
        learn_from_failure(ch, gsn_blackjack);
        send_to_char("You failed!\n\r", ch);
        global_retcode = damage(ch, victim, 0, gsn_blackjack);
    }
    return;
}


/* combat cure poison by Arcturus */
void do_flushpoison( CHAR_DATA * ch, const char *argument)
{
    short percent;
    OBJ_DATA* wobj;

    if (!IS_NPC(ch))
    {
        if (IS_SET(ch->pcdata->flags, PCFLAG_BOUND))
        {
            send_to_char("How do you intend to do that when bound?\n\r", ch);
            return;
        }
    }

    if (ch->fighting)
    {
        send_to_char("You can't concentrate enough while you are fighting.\n\r", ch);
        return;
    }
    for (wobj = ch->first_carrying; wobj; wobj = wobj->next_content)
    {
        if (wobj->item_type == ITEM_DRINK_CON
            && wobj->value[1] > 0
            && wobj->value[2] == 0)
            break;
    }
    if (!wobj)
    {
        send_to_char("With what water?\n\r", ch);
        return;
    }
    if (is_affected(ch, gsn_poison))
    {
        wobj->value[1] -= 1;
        percent = IS_NPC(ch) ? 80 : ch->pcdata->learned[gsn_flushpoison];
        percent = UMIN(75, percent);
        if (!chance(ch, percent))
        {
            send_to_char("You fail to concentrate.", ch);
            learn_from_failure(ch, gsn_flushpoison);
            return;
        }
        affect_strip(ch, gsn_poison);
        set_char_color(AT_MAGIC, ch);
        send_to_char("You concentrate and flush the poison from your body.\n\r", ch);
        ch->mental_state = URANGE(-100, ch->mental_state, -10);
        learn_from_success(ch, gsn_flushpoison);
        return;
    }
    else
    {
        send_to_char("You aren't poisoned.\n\r", ch);
        return;
    }
}

/* BINDING BY ARCTURUS */

void do_unbind( CHAR_DATA * ch, const char *argument)
{
    //char buf[MAX_STRING_LENGTH];
    char arg1[MAX_INPUT_LENGTH];
    CHAR_DATA* victim;

    argument = one_argument(argument, arg1);

    if (!IS_NPC(ch))
    {
        if (IS_SET(ch->pcdata->flags, PCFLAG_BOUND))
        {
            send_to_char("Not when your bound you aren't.\n\r", ch);
            return;
        }
    }

    if (ch->mount)
    {
        send_to_char("You can't do that while mounted.\n\r", ch);
        return;
    }
    if (ch->fighting)
    {
        send_to_char("You are a little occupied at the moment.\n\r", ch);
        return;
    }

    if (arg1[0] == '\0')
    {

        send_to_char("Unbind Who?\n\r", ch);
        return;
    }

    if ((victim = get_char_room(ch, arg1)) == NULL)
    {
        send_to_char("They aren't here.\n\r", ch);
        return;
    }

    if (victim == ch)
    {
        send_to_char("How do you intend to do that?\n\r", ch);
        return;
    }
    if (IS_NPC(victim))
    {
        send_to_char("You can't unbind a mob.\n\r", ch);
        return;
    }
    else
    {
        if (IS_SET(victim->pcdata->flags, PCFLAG_BOUND))
        {
            REMOVE_BIT(victim->pcdata->flags, PCFLAG_BOUND);
            send_to_char("You carefully remove the bindings.\n\r", ch);
            act(AT_ACTION, "$n has removed your bindings!\n\r", ch, NULL, victim, TO_VICT);
            act(AT_ACTION, "$n has removed $N's bindings.\n\r", ch, NULL, victim, TO_NOTVICT);
            return;
        }
        send_to_char("They aren't bound.\n\r", ch);
        return;
    }
}

/* BINDING BY ARCTURUS */
void do_bind( CHAR_DATA * ch, const char *argument)
{
    //char buf[MAX_STRING_LENGTH];
    char logbuf[MAX_STRING_LENGTH];
    char arg1[MAX_INPUT_LENGTH];
    CHAR_DATA* victim;
    int percent, bindchance;

    argument = one_argument(argument, arg1);

    if (!IS_NPC(ch))
    {
        if (IS_SET(ch->pcdata->flags, PCFLAG_BOUND))
        {
            send_to_char("Nice try, your already bound.\n\r", ch);
            return;
        }
    }

    if (ch->mount)
    {
        send_to_char("You can't do that while mounted.\n\r", ch);
        return;
    }

    if (arg1[0] == '\0')
    {

        send_to_char("Bind whom?\n\r", ch);
        return;
    }

    if ((victim = get_char_room(ch, arg1)) == NULL)
    {
        send_to_char("They aren't here.\n\r", ch);
        return;
    }
    if (ch->fighting)
    {
        send_to_char("You can't get away from the fight long enough.\n\r", ch);
        return;
    }

    if (victim == ch)
    {
        send_to_char("That's pointless.\n\r", ch);
        return;
    }
    if (IS_NPC(victim))
    {
        send_to_char("You can't bind a mob.\n\r", ch);
        return;
    }
    if (IS_SET(victim->pcdata->flags, PCFLAG_BOUND))
    {
        send_to_char("They already appear to be bound.\n\r", ch);
        return;
    }
    if (victim->fighting)
    {
        send_to_char("You can't get in close enough while they are fighting.\n\r", ch);
        return;
    }
    percent = number_percent();
    bindchance = ch->pcdata->learned[gsn_bind];
    bindchance *= .6;
    sprintf(logbuf, "%s binding %s (%d/%d)", ch->name, victim->name, percent, bindchance);
    log_string(logbuf);


    if (victim->position <= POS_SLEEPING) //sleeping/stunned bonus.
    {
        bindchance += 75;
        sprintf(logbuf, "Sleeping penalty  for %s", victim->name);
        log_string(logbuf);
    }
    if (!can_see(victim, ch))
    {
        bindchance += 50;
        sprintf(logbuf, "Invis penalty  for %s", victim->name);
        log_string(logbuf);
    }
    if (victim->perm_frc > 0)  //forceuser penalty (5 frc(forcechosen) == 15% bonus)
    {
        bindchance -= (victim->perm_frc * 3);
        sprintf(logbuf, "Forcer Bonus for %s -- %d frc (%d)", victim->name, victim->perm_frc, bindchance);
        log_string(logbuf);
    }

    sprintf(logbuf, "%s binding %s (%d/%d)", ch->name, victim->name, percent, bindchance);
    log_string(logbuf);
    if (percent > bindchance)
    {
        /*
         * Failure.
         */
        send_to_char("Ahh shoot....\n\r", ch);
        act(AT_ACTION, "$n tried to put you in binders!\n\r", ch, NULL, victim, TO_VICT);
        act(AT_ACTION, "$n tried to bind $N.\n\r", ch, NULL, victim, TO_NOTVICT);
        learn_from_failure(ch, gsn_bind);
        sprintf(logbuf, "%s failed binding %s", ch->name, victim->name);
        log_string(logbuf);
        return;
    }
    SET_BIT(victim->pcdata->flags, PCFLAG_BOUND);
    sprintf(logbuf, "%s successfully bound %s", ch->name, victim->name);
    log_string(logbuf);
    send_to_char("You put them in binders successfully.\n\r", ch);
    act(AT_ACTION, "$n has put you in binders!\n\r", ch, NULL, victim, TO_VICT);
    act(AT_ACTION, "$n has put $N in binders!\n\r", ch, NULL, victim, TO_NOTVICT);
    gain_exp(ch, 2500, LEADERSHIP_ABILITY);
    learn_from_success(ch, gsn_bind);
    return;
}


/* The command used to attempt a masked get- Arcturus */
void do_palm( CHAR_DATA * ch, const char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    OBJ_DATA* obj;
    OBJ_DATA* obj_next;
    OBJ_DATA* container;
    short number;
    bool found;

    argument = one_argument(argument, arg1);
    if (is_number(arg1))
    {
        number = atoi(arg1);
        if (number < 1)
        {
            send_to_char("That was easy...\n\r", ch);
            return;
        }
        if ((ch->carry_number + number) > can_carry_n(ch))
        {
            send_to_char("You can't carry that many.\n\r", ch);
            return;
        }
        argument = one_argument(argument, arg1);
    }
    else
        number = 0;
    argument = one_argument(argument, arg2);
    /* munch optional words */
    if (!str_cmp(arg2, "from") && argument[0] != '\0')
        argument = one_argument(argument, arg2);

    /* Get type. */
    if (arg1[0] == '\0')
    {
        send_to_char("Palm what?\n\r", ch);
        return;
    }

    if (ms_find_obj(ch))
        return;

    if (arg2[0] == '\0')
    {
        if (number <= 1 && str_cmp(arg1, "all") && str_prefix("all.", arg1))
        {
            /* 'get obj' */
            obj = get_obj_list(ch, arg1, ch->in_room->first_content);
            if (!obj)
            {
                act(AT_PLAIN, "I see no $T here.", ch, NULL, arg1, TO_CHAR);
                return;
            }
            separate_obj(obj);
            get_obj_palm(ch, obj, NULL);
            if (char_died(ch))
                return;
            if (IS_SET(sysdata.save_flags, SV_GET))
                save_char_obj(ch);
        }
        else
        {
            short cnt = 0;
            bool fAll;
            char* chk;

            if (IS_SET(ch->in_room->room_flags, ROOM_DONATION))
            {
                send_to_char("The gods frown upon such a display of greed!\n\r", ch);
                return;
            }
            if (!str_cmp(arg1, "all"))
                fAll = TRUE;
            else
                fAll = FALSE;
            if (number > 1)
                chk = arg1;
            else
                chk = &arg1[4];
            /* 'get all' or 'get all.obj' */
            found = FALSE;
            for (obj = ch->in_room->first_content; obj; obj = obj_next)
            {
                obj_next = obj->next_content;
                if ((fAll || nifty_is_name(chk, obj->name))
                    && can_see_obj(ch, obj))
                {
                    found = TRUE;
                    if (number && (cnt + obj->count) > number)
                        split_obj(obj, number - cnt);
                    cnt += obj->count;
                    get_obj_palm(ch, obj, NULL);
                    if (char_died(ch)
                        || ch->carry_number >= can_carry_n(ch)
                        || ch->carry_weight >= can_carry_w(ch)
                        || (number && cnt >= number))
                    {
                        if (IS_SET(sysdata.save_flags, SV_GET)
                            && !char_died(ch))
                            save_char_obj(ch);
                        return;
                    }
                }
            }

            if (!found)
            {
                if (fAll)
                    send_to_char("I see nothing here.\n\r", ch);
                else
                    act(AT_PLAIN, "I see no $T here.", ch, NULL, chk, TO_CHAR);
            }
            else
                if (IS_SET(sysdata.save_flags, SV_GET))
                    save_char_obj(ch);
        }
    }
    else
    {
        /* 'get ... container' */
        if (!str_cmp(arg2, "all") || !str_prefix("all.", arg2))
        {
            send_to_char("You can't do that.\n\r", ch);
            return;
        }

        if ((container = get_obj_here(ch, arg2)) == NULL)
        {
            act(AT_PLAIN, "I see no $T here.", ch, NULL, arg2, TO_CHAR);
            return;
        }

        switch (container->item_type)
        {
        default:
            if (!IS_OBJ_STAT(container, ITEM_COVERING))
            {
                send_to_char("That's not a container.\n\r", ch);
                return;
            }
            if (ch->carry_weight + container->weight > can_carry_w(ch))
            {
                send_to_char("It's too heavy for you to lift.\n\r", ch);
                return;
            }
            break;

        case ITEM_CONTAINER:
        case ITEM_DROID_CORPSE:
        case ITEM_CORPSE_PC:
        case ITEM_CORPSE_NPC:
            break;
        }

        if (!IS_OBJ_STAT(container, ITEM_COVERING)
            && IS_SET(container->value[1], CONT_CLOSED))
        {
            act(AT_PLAIN, "The $d is closed.", ch, NULL, container->name, TO_CHAR);
            return;
        }

        if (number <= 1 && str_cmp(arg1, "all") && str_prefix("all.", arg1))
        {
            /* 'get obj container' */
            obj = get_obj_list(ch, arg1, container->first_content);
            if (!obj)
            {
                act(AT_PLAIN, IS_OBJ_STAT(container, ITEM_COVERING) ?
                    "I see nothing like that beneath the $T." :
                    "I see nothing like that in the $T.",
                    ch, NULL, arg2, TO_CHAR);
                return;
            }
            separate_obj(obj);
            get_obj_palm(ch, obj, container);

            check_for_trap(ch, container, TRAP_GET);
            if (char_died(ch))
                return;
            if (IS_SET(sysdata.save_flags, SV_GET))
                save_char_obj(ch);
        }
        else
        {
            int cnt = 0;
            bool fAll;
            char* chk;

            /* 'get all container' or 'get all.obj container' */
    /*	    if ( IS_OBJ_STAT( container, ITEM_DONATION ) )
            {
            send_to_char( "The gods frown upon such an act of greed!\n\r", ch );
            return;
            }*/
            if (!str_cmp(arg1, "all"))
                fAll = TRUE;
            else
                fAll = FALSE;
            if (number > 1)
                chk = arg1;
            else
                chk = &arg1[4];
            found = FALSE;
            for (obj = container->first_content; obj; obj = obj_next)
            {
                obj_next = obj->next_content;
                if ((fAll || nifty_is_name(chk, obj->name))
                    && can_see_obj(ch, obj))
                {
                    found = TRUE;
                    if (number && (cnt + obj->count) > number)
                        split_obj(obj, number - cnt);
                    cnt += obj->count;
                    get_obj_palm(ch, obj, container);
                    if (char_died(ch)
                        || ch->carry_number >= can_carry_n(ch)
                        || ch->carry_weight >= can_carry_w(ch)
                        || (number && cnt >= number))
                        return;
                }
            }

            if (!found)
            {
                if (fAll)
                    act(AT_PLAIN, IS_OBJ_STAT(container, ITEM_COVERING) ?
                        "I see nothing beneath the $T." :
                        "I see nothing in the $T.",
                        ch, NULL, arg2, TO_CHAR);
                else
                    act(AT_PLAIN, IS_OBJ_STAT(container, ITEM_COVERING) ?
                        "I see nothing like that beneath the $T." :
                        "I see nothing like that in the $T.",
                        ch, NULL, arg2, TO_CHAR);
            }
            else
                check_for_trap(ch, container, TRAP_GET);
            if (char_died(ch))
                return;
            if (found && IS_SET(sysdata.save_flags, SV_GET))
                save_char_obj(ch);
        }
    }
    return;
}

/* Palming it anyone? Masked Get */
void get_obj_palm( CHAR_DATA * ch, OBJ_DATA* obj, OBJ_DATA* container)
{
    CLAN_DATA* clan;
    int weight;
    int chance, percent;
    CHAR_DATA* rch;
    chance = IS_NPC(ch) ? 50
        : ch->pcdata->learned[gsn_palm] * .75 + ch->mod_dex / 2;
    if (!CAN_WEAR(obj, ITEM_TAKE)
        && (ch->top_level < sysdata.level_getobjnotake))
    {
        send_to_char("You can't take that.\n\r", ch);
        return;
    }
    if (IS_OBJ_STAT(obj, ITEM_PROTOTYPE)
        && !can_take_proto(ch))
    {
        send_to_char("A godly force prevents you from getting close to it.\n\r", ch);
        return;
    }

    if (ch->carry_number + get_obj_number(obj) > can_carry_n(ch))
    {
        act(AT_PLAIN, "$d: you can't carry that many items.",
            ch, NULL, obj->name, TO_CHAR);
        return;
    }

    if (IS_OBJ_STAT(obj, ITEM_COVERING))
        weight = obj->weight;
    else
        weight = get_obj_weight(obj);

    if (ch->carry_weight + weight > can_carry_w(ch))
    {
        act(AT_PLAIN, "$d: you can't carry that much weight.",
            ch, NULL, obj->name, TO_CHAR);
        return;
    }
    if (container)
    {
        act(AT_ACTION, IS_OBJ_STAT(container, ITEM_COVERING) ?
            "You palm $p from beneath $P." : "You palm $p from $P",
            ch, obj, container, TO_CHAR);
        /* Not everyone can always catch a failed palm attempt, we'll limit it. */
        for (rch = ch->in_room->first_person; rch; rch = rch->next_in_room)
        {
            percent = number_percent();
            if (!can_see(rch, ch))
                percent -= 25; /* Kinda hard to detect someone you can't see. */
            if (rch == ch) /* blah that I forgot this. */
                continue;
            if (IS_AWAKE(rch))
            {
                if (IS_AFFECTED(rch, AFF_SIT_AWARE))
                {
                    percent += 15;
                }
            }
            else /* How are you going to see someone get something when you are asleep? */
            {
                percent = 0;
            }
            if (IS_IMMORTAL(rch) && IS_AWAKE(rch)) /* You aren't sneaking anything by an immortal. */
                percent += 100;
            if (percent > chance)
            {
                set_char_color(AT_ACTION, rch);
                if (IS_OBJ_STAT(container, ITEM_COVERING))
                    ch_printf(rch, "%s gets %s from beneath %s.\n\r", NAME(ch), obj->short_descr, container->short_descr);
                else
                    ch_printf(rch, "%s gets %s from %s.\n\r", NAME(ch), obj->short_descr, container->short_descr);
                learn_from_failure(ch, gsn_palm);
            }
            else
                learn_from_success(ch, gsn_palm);
        }
        obj_from_obj(obj);
    }
    else
    {
        act(AT_ACTION, "You palm $p.", ch, obj, container, TO_CHAR);
        /* Not everyone can always catch a failed palm attempt, we'll limit it. */
        for (rch = ch->in_room->first_person; rch; rch = rch->next_in_room)
        {
            percent = number_percent();
            if (!can_see(rch, ch))
                percent -= 25; /* Kinda hard to detect someone you can't see. */
            if (rch == ch) /* Damn me for forgetting this again. */
                continue;
            if (IS_AWAKE(rch))
            {
                if (IS_AFFECTED(rch, AFF_SIT_AWARE))
                {
                    percent += 15;
                }
            }
            else /* How are you going to see someone get something when you are asleep? */
            {
                percent = 0;
            }
            if (IS_IMMORTAL(rch) && IS_AWAKE(rch)) /* You aren't sneaking anything by an immortal. */
                percent += 100;
            if (percent > chance)
            {
                set_char_color(AT_ACTION, rch);
                ch_printf(rch, "%s gets %s.", NAME(ch), obj->short_descr);
                learn_from_failure(ch, gsn_palm);
            }
            else
                learn_from_success(ch, gsn_palm);
        }
        obj_from_room(obj);
    }

    /* Clan storeroom checks */
    if (IS_SET(ch->in_room->room_flags, ROOM_CLANSTOREROOM)
        && (!container || container->carried_by == NULL))
        for (clan = first_clan; clan; clan = clan->next)
            if (clan->storeroom == ch->in_room->vnum)
                save_clan_storeroom(ch, clan);

    if (obj->item_type != ITEM_CONTAINER)
        check_for_trap(ch, obj, TRAP_GET);
    if (char_died(ch))
        return;
    if (obj->item_type == ITEM_MONEY)
    {
        ch->gold += obj->value[0];
        extract_obj(obj);
    }
    else
    {
        obj = obj_to_char(obj, ch);
    }

    if (char_died(ch) || obj_extracted(obj))
        return;
    oprog_get_trigger(ch, obj);
    return;
}

/* Tuck it away, without being seen. Redone put with mask chance. -Arcturus */
void do_tuck(CHAR_DATA * ch, const char *argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    OBJ_DATA* container;
    OBJ_DATA* obj;
    OBJ_DATA* obj_next;
    CLAN_DATA* clan;
    short      count;
    int         number;
    bool        save_char = FALSE;
    int percent, chance;
    CHAR_DATA* rch;
    chance = IS_NPC(ch) ? 50
        : ch->pcdata->learned[gsn_tuck] * .75 + ch->mod_dex / 2;
    argument = one_argument(argument, arg1);
    if (is_number(arg1))
    {
        number = atoi(arg1);
        if (number < 1)
        {
            send_to_char("That was easy...\n\r", ch);
            return;
        }
        argument = one_argument(argument, arg1);
    }
    else
        number = 0;
    argument = one_argument(argument, arg2);
    /* munch optional words */
    if ((!str_cmp(arg2, "into") || !str_cmp(arg2, "inside") || !str_cmp(arg2, "in"))
        && argument[0] != '\0')
        argument = one_argument(argument, arg2);

    if (arg1[0] == '\0' || arg2[0] == '\0')
    {
        send_to_char("Tuck what in what?\n\r", ch);
        return;
    }

    if (ms_find_obj(ch))
        return;

    if (!str_cmp(arg2, "all") || !str_prefix("all.", arg2))
    {
        send_to_char("You can't do that.\n\r", ch);
        return;
    }

    if ((container = get_obj_here(ch, arg2)) == NULL)
    {
        act(AT_PLAIN, "I see no $T here.", ch, NULL, arg2, TO_CHAR);
        return;
    }

    if (!container->carried_by && IS_SET(sysdata.save_flags, SV_PUT))
        save_char = TRUE;

    if (IS_OBJ_STAT(container, ITEM_COVERING))
    {
        if (ch->carry_weight + container->weight > can_carry_w(ch))
        {
            send_to_char("It's too heavy for you to lift.\n\r", ch);
            return;
        }
    }
    else
    {
        if (container->item_type != ITEM_CONTAINER)
        {
            send_to_char("That's not a container.\n\r", ch);
            return;
        }

        if (IS_SET(container->value[1], CONT_CLOSED))
        {
            act(AT_PLAIN, "The $d is closed.", ch, NULL, container->name, TO_CHAR);
            return;
        }
    }

    if (number <= 1 && str_cmp(arg1, "all") && str_prefix("all.", arg1))
    {
        /* 'put obj container' */
        if ((obj = get_obj_carry(ch, arg1)) == NULL)
        {
            send_to_char("You do not have that item.\n\r", ch);
            return;
        }

        if (obj == container)
        {
            send_to_char("You can't fold it into itself.\n\r", ch);
            return;
        }

        if (!can_drop_obj(ch, obj))
        {
            send_to_char("You can't let go of it.\n\r", ch);
            return;
        }

        if ((IS_OBJ_STAT(container, ITEM_COVERING)
            && (get_obj_weight(obj) / obj->count)
          > ((get_obj_weight(container) / container->count)
              - container->weight)))
        {
            send_to_char("It won't fit under there.\n\r", ch);
            return;
        }

        if ((get_obj_weight(obj) / obj->count)
            + (get_obj_weight(container) / container->count)
           > container->value[0])
        {
            send_to_char("It won't fit.\n\r", ch);
            return;
        }

        separate_obj(obj);
        separate_obj(container);
        obj_from_char(obj);
        obj = obj_to_obj(obj, container);
        check_for_trap(ch, container, TRAP_PUT);
        if (char_died(ch))
            return;
        count = obj->count;
        obj->count = 1;
        act(AT_ACTION, IS_OBJ_STAT(container, ITEM_COVERING)
            ? "You tuck $p beneath $P." : "You tuck $p in $P.",
            ch, obj, container, TO_CHAR);
        for (rch = ch->in_room->first_person; rch; rch = rch->next_in_room)
        {
            percent = number_percent();
            if (!can_see(rch, ch))
                percent -= 25; /* Kinda hard to detect someone you can't see. */
            if (rch == ch) /* Damn me for forgetting this again. */
                continue;
            if (IS_AWAKE(rch))
            {
                if (IS_AFFECTED(rch, AFF_SIT_AWARE))
                {
                    percent += 15;
                }
            }
            else /* How are you going to see someone get something when you are asleep? */
            {
                percent = 0;
            }
            if (IS_IMMORTAL(rch) && IS_AWAKE(rch)) /* You aren't sneaking anything by an immortal. */
                percent += 100;
            if (percent > chance)
            {
                set_char_color(AT_ACTION, rch);
                if (IS_OBJ_STAT(container, ITEM_COVERING))
                    ch_printf(rch, "%s hides %s beneath %s.\n\r", NAME(ch), obj->short_descr, container->short_descr);
                else
                    ch_printf(rch, "%s puts %s in %s.\n\r", NAME(ch), obj->short_descr, container->short_descr);
                learn_from_failure(ch, gsn_tuck);
            }
            else
                learn_from_success(ch, gsn_tuck);
        }
        obj->count = count;

        if (save_char)
            save_char_obj(ch);
        /* Clan storeroom check */
        if (IS_SET(ch->in_room->room_flags, ROOM_CLANSTOREROOM)
            && container->carried_by == NULL)
            for (clan = first_clan; clan; clan = clan->next)
                if (clan->storeroom == ch->in_room->vnum)
                    save_clan_storeroom(ch, clan);
    }
    else
    {
        bool found = FALSE;
        int cnt = 0;
        bool fAll;
        char* chk;

        if (!str_cmp(arg1, "all"))
            fAll = TRUE;
        else
            fAll = FALSE;
        if (number > 1)
            chk = arg1;
        else
            chk = &arg1[4];

        separate_obj(container);
        /* 'put all container' or 'put all.obj container' */
        for (obj = ch->first_carrying; obj; obj = obj_next)
        {
            obj_next = obj->next_content;

            if ((fAll || nifty_is_name(chk, obj->name))
                && can_see_obj(ch, obj)
                && obj->wear_loc == WEAR_NONE
                && obj != container
                && can_drop_obj(ch, obj)
                && get_obj_weight(obj) + get_obj_weight(container)
                <= container->value[0])
            {
                if (number && (cnt + obj->count) > number)
                    split_obj(obj, number - cnt);
                cnt += obj->count;
                obj_from_char(obj);
                act(AT_ACTION, "You tuck $p in $P.", ch, obj, container, TO_CHAR);
                for (rch = ch->in_room->first_person; rch; rch = rch->next_in_room)
                {
                    percent = number_percent();
                    if (!can_see(rch, ch))
                        percent -= 25; /* Kinda hard to detect someone you can't see. */
                    if (rch == ch) /* Damn me for forgetting this again. */
                        continue;
                    if (IS_AWAKE(rch))
                    {
                        if (IS_AFFECTED(rch, AFF_SIT_AWARE))
                        {
                            percent += 15;
                        }
                    }
                    else /* How are you going to see someone get something when you are asleep? */
                    {
                        percent = 0;
                    }
                    if (IS_IMMORTAL(rch) && IS_AWAKE(rch)) /* You aren't sneaking anything by an immortal. */
                        percent += 100;
                    if (percent > chance)
                    {
                        set_char_color(AT_ACTION, rch);
                        ch_printf(rch, "%s puts %s in %s.\n\r", NAME(ch), obj->short_descr, container->short_descr);
                        learn_from_failure(ch, gsn_tuck);
                    }
                    else
                        learn_from_success(ch, gsn_tuck);
                }
                obj = obj_to_obj(obj, container);
                found = TRUE;

                check_for_trap(ch, container, TRAP_PUT);
                if (char_died(ch))
                    return;
                if (number && cnt >= number)
                    break;
            }
        }

        /*
         * Don't bother to save anything if nothing was dropped   -Thoric
         */
        if (!found)
        {
            if (fAll)
                act(AT_PLAIN, "You are not carrying anything.",
                    ch, NULL, NULL, TO_CHAR);
            else
                act(AT_PLAIN, "You are not carrying any $T.",
                    ch, NULL, chk, TO_CHAR);
            return;
        }
        if (save_char)
            save_char_obj(ch);
        /* Clan storeroom check */
        if (IS_SET(ch->in_room->room_flags, ROOM_CLANSTOREROOM)
            && container->carried_by == NULL)
            for (clan = first_clan; clan; clan = clan->next)
                if (clan->storeroom == ch->in_room->vnum)
                    save_clan_storeroom(ch, clan);
    }
    return;
}


void do_snipe( CHAR_DATA * ch, const char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    short dir, dist;
    short max_dist = 3;
    EXIT_DATA* pexit;
    ROOM_INDEX_DATA* was_in_room;
    ROOM_INDEX_DATA* to_room;
    ROOM_INDEX_DATA* watched;
    CHAR_DATA* victim = NULL;
    int schance;
    //ch_ret dummy;
    char buf[MAX_STRING_LENGTH];
    bool pfound = FALSE;
    bool watching = FALSE;
    OBJ_DATA* wield;
    OBJ_DATA* obj;

    if (!IS_NPC(ch))
    {
        if (IS_SET(ch->pcdata->flags, PCFLAG_BOUND))
        {
            send_to_char("How do you intend to do that when bound?\n\r", ch);
            return;
        }
    }

    if (ch->watching)
    {
        watched = ch->watching;
        watching = TRUE;
    }

    if (!IS_NPC(ch))
    {
        if (IS_SET(ch->pcdata->flags, PCFLAG_BOUND))
        {
            send_to_char("Not when you are bound up.\n\r", ch);
            return;
        }
    }

    if (ch->subclass == SUBCLASS_SNIPER)
        max_dist = 10;

    if (IS_SET(ch->in_room->room_flags, ROOM_SAFE))
    {
        set_char_color(AT_MAGIC, ch);
        send_to_char("You'll have to do that elswhere.\r\n", ch);
        return;
    }

    if (get_eq_char(ch, WEAR_DUAL_WIELD) != NULL)
    {
        send_to_char("You can't do that while wielding two weapons.", ch);
        return;
    }
    /*
    wield = get_eq_char( ch, WEAR_WIELD );
    if( !wield || wield->item_type != ITEM_WEAPON || wield->value[3] != WEAPON_BLASTER ) */
    wield = get_eq_char(ch, WEAR_WIELD);
    if (!wield || wield->item_type != ITEM_WEAPON || (wield->value[3] != WEAPON_BLASTER
        && wield->value[3] != WEAPON_SLUGTHROWER && wield->value[3] != WEAPON_BOWCASTER))
    {
        send_to_char("You don't seem to be holding a blaster", ch);
        return;
    }

    argument = one_argument(argument, arg);
    argument = one_argument(argument, arg2);

    if ((dir = get_door(arg)) == -1 || arg2[0] == '\0')
    {
        send_to_char("Usage: snipe <dir> <target>\r\n", ch);
        return;
    }

    if ((pexit = get_exit(ch->in_room, dir)) == NULL)
    {
        send_to_char("Are you expecting to fire through a wall!?\r\n", ch);
        return;
    }

    if (IS_SET(pexit->exit_info, EX_CLOSED))
    {
        send_to_char("Are you expecting to fire through a door!?\r\n", ch);
        return;
    }

    was_in_room = ch->in_room;

    for (dist = 0; dist <= max_dist; dist++)
    {
        if (IS_SET(pexit->exit_info, EX_CLOSED))
            break;

        if (!pexit->to_room)
            break;

        to_room = NULL;
        if (pexit->distance > 1)
            to_room = generate_exit(ch->in_room, &pexit);

        if (to_room == NULL)
            to_room = pexit->to_room;

        char_from_room(ch);
        char_to_room(ch, to_room);


        if (IS_NPC(ch) && (victim = get_char_room_mp(ch, arg2)) != NULL)
        {
            pfound = TRUE;
            break;
        }
        else if (!IS_NPC(ch) && (victim = get_char_room(ch, arg2)) != NULL)
        {
            pfound = TRUE;
            break;
        }


        if ((pexit = get_exit(ch->in_room, dir)) == NULL)
            break;

    }

    char_from_room(ch);
    char_to_room(ch, was_in_room);

    if (!pfound)
    {
        ch_printf(ch, "You don't see that person to the %s!\r\n", dir_name[dir]);
        char_from_room(ch);
        char_to_room(ch, was_in_room);
        return;
    }

    if (victim == ch)
    {
        send_to_char("Shoot yourself ... really?\r\n", ch);
        return;
    }

    if (IS_SET(victim->in_room->room_flags, ROOM_SAFE))
    {
        set_char_color(AT_MAGIC, ch);
        send_to_char("You can't shoot them there.\r\n", ch);
        return;
    }

    if (is_safe(ch, victim))
        return;

    if (IS_AFFECTED(ch, AFF_CHARM) && ch->master == victim)
    {
        act(AT_PLAIN, "$N is your beloved master.", ch, NULL, victim, TO_CHAR);
        return;
    }

    if (ch->position == POS_FIGHTING)
    {
        send_to_char("You do the best you can!\r\n", ch);
        return;
    }

    if (!IS_NPC(victim) && IS_SET(ch->act, PLR_NICE))
    {
        send_to_char("You feel too nice to do that!\r\n", ch);
        return;
    }

    schance = IS_NPC(ch) ? 100 : (int)(ch->pcdata->learned[gsn_snipe]);

    switch (dir)
    {
    case 0:
    case 1:
        dir += 2;
        break;
    case 2:
    case 3:
        dir -= 2;
        break;
    case 4:
    case 7:
        dir += 1;
        break;
    case 5:
    case 8:
        dir -= 1;
        break;
    case 6:
        dir += 3;
        break;
    case 9:
        dir -= 3;
        break;
    }

    /*Removed from SWRFUSS*/
    /*if (number_percent() < schance)
    {
        char_from_room(ch);
        char_to_room(ch, was_in_room);
        snprintf(buf, MAX_STRING_LENGTH, "$n fires a blaster shot to the %s.", dir_name[get_door(arg)]);
        act(AT_ACTION, buf, ch, NULL, NULL, TO_ROOM);

        char_from_room(ch);
        char_to_room(ch, victim->in_room);
    }*/

    char_from_room( ch ); /*added from SWL*/
    char_to_room( ch, victim->in_room ); 

    if( number_percent() < schance) /*added from SWL*/
    {
        if( ch->subclass != SUBCLASS_SNIPER )
        {        
            sprintf(buf, "A blaster shot fires at you from the %s.", dir_name[dir]);
            act(AT_ACTION, buf, victim, NULL, ch, TO_CHAR);
            act(AT_ACTION, "You fire at $N.", ch, NULL, victim, TO_CHAR);
            sprintf(buf, "A blaster shot fires at $N from the %s.", dir_name[dir]);
            act(AT_ACTION, buf, ch, NULL, victim, TO_NOTVICT);
            one_hit(ch, victim, TYPE_UNDEFINED);
        
        }  
        else
        {
            //if (ch->sniperweapon != 0 && number_percent() <= 5)
            if((( obj = get_eq_char(ch, WEAR_WIELD)) == NULL || (obj->value[3] != WEAPON_SLUGTHROWER)) && number_percent() <= 5 )
            {
                act(AT_ACTION, "You Whisper, one Shot, One Kill as you line $N in your scope.", ch, NULL, victim, TO_CHAR);
                if (!IS_IMMORTAL(victim))
                    raw_kill(ch, victim);
                else
                    /*    dummy = noparry_damage(ch, victim, 32700, TYPE_HIT + wield->value[3]);
                    char_from_room(ch);
                    char_to_room(ch, was_in_room);
                    */
                    //dummy = damage(ch, victim, 32700, gsn_snipe);
                char_from_room(ch);
                char_to_room(ch, was_in_room);
                if (watching)
                    start_watching(ch, watched);

                return;
            }
            /*sprintf(buf, MAX_STRING_LENGTH, "A blaster shot fires at you from the %s.", dir_name[dir]);*/
            /*act(AT_ACTION, buf, victim, NULL, ch, TO_CHAR);*/
            sprintf(buf, "A flurry of shots fire at you from the %s.", dir_name[dir]);
            act(AT_ACTION, buf, victim, NULL, ch, TO_CHAR);
            act(AT_ACTION, "You squeeze three shots off at $N.", ch, NULL, victim, TO_CHAR);
            sprintf(buf, "A trio of shots fires at $N from the %s.", dir_name[dir]);
            act(AT_ACTION, buf, ch, NULL, victim, TO_NOTVICT);

            one_hit(ch, victim, TYPE_UNDEFINED);
            one_hit(ch, victim, TYPE_UNDEFINED);
            one_hit(ch, victim, TYPE_UNDEFINED);
        }
        if (char_died(ch))
            return;

        stop_fighting(ch, TRUE);

        learn_from_success(ch, gsn_snipe);
    }
    else
    {
        char_from_room(ch);
        char_to_room(ch, was_in_room);
        snprintf(buf, MAX_STRING_LENGTH, "$n fires a blaster shot to the %s.", dir_name[get_door(arg)]);
        act(AT_ACTION, buf, ch, NULL, NULL, TO_ROOM);

        char_from_room(ch);
        char_to_room(ch, victim->in_room);

        act(AT_ACTION, "You fire at $N but don't even come close.", ch, NULL, victim, TO_CHAR);
        snprintf(buf, MAX_STRING_LENGTH, "A blaster shot fired from the %s barely misses you.", dir_name[dir]);
        act(AT_ACTION, buf, ch, NULL, victim, TO_ROOM);
        learn_from_failure(ch, gsn_snipe);
    }

    char_from_room(ch);
    char_to_room(ch, was_in_room);

    if (IS_NPC(ch))
        WAIT_STATE(ch, 1 * PULSE_VIOLENCE);
    else
    {
        if (number_percent() < ch->pcdata->learned[gsn_third_attack])
            WAIT_STATE(ch, 1 * PULSE_PER_SECOND);
        else if (number_percent() < ch->pcdata->learned[gsn_second_attack])
            WAIT_STATE(ch, 2 * PULSE_PER_SECOND);
        else
            WAIT_STATE(ch, 3 * PULSE_PER_SECOND);
    }
    if (IS_NPC(victim) && !char_died(victim))
    {
        if (IS_SET(victim->act, ACT_SENTINEL))
        {
            victim->was_sentinel = victim->in_room;
            REMOVE_BIT(victim->act, ACT_SENTINEL);
        }

        start_hating(victim, ch);
        start_hunting(victim, ch);

    }
}


/* Slide-By Arcturus */
void do_slide(CHAR_DATA* ch, const char* argument)
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    char buf[MAX_INPUT_LENGTH];
    CHAR_DATA* victim;
    CHAR_DATA* rch;
    OBJ_DATA* obj;
    int chance, percent;

    chance = IS_NPC(ch) ? 50
        : ch->pcdata->learned[gsn_slide] *.75 + ch->mod_dex / 2;

    argument = one_argument(argument, arg1);
    argument = one_argument(argument, arg2);
    if (!str_cmp(arg2, "to") && argument[0] != '\0')
        argument = one_argument(argument, arg2);

    if (arg1[0] == '\0' || arg2[0] == '\0')
    {
        send_to_char("Slide what to whom?\n\r", ch);
        return;
    }

    if (ms_find_obj(ch))
        return;
    if (is_number(arg1))
    {
        /* 'give NNNN coins victim' */
        int amount;

        amount = atoi(arg1);
        if (amount <= 0
            || (str_cmp(arg2, "credits") && str_cmp(arg2, "credit")))
        {
            send_to_char("Sorry, you can't do that.\n\r", ch);
            return;
        }

        argument = one_argument(argument, arg2);
        if (!str_cmp(arg2, "to") && argument[0] != '\0')
            argument = one_argument(argument, arg2);
        if (arg2[0] == '\0')
        {
            send_to_char("Slide what to whom?\n\r", ch);
            return;
        }

        if ((victim = get_char_room(ch, arg2)) == NULL)
        {
            send_to_char("They aren't here.\n\r", ch);
            return;
        }

        if (ch->gold < amount)
        {
            send_to_char("Very generous of you, but you haven't got that many credits.\n\r", ch);
            return;
        }

        ch->gold -= amount;
        victim->gold += amount;
        strcpy(buf, "$n gives you ");
        strcat(buf, arg1);
        strcat(buf, (amount > 1) ? " credits." : " credit.");
        set_char_color(AT_ACTION, ch);
        ch_printf(ch, "You slide %s some credits.\n\r", NAME(victim));
        for (rch = ch->in_room->first_person; rch; rch = rch->next_in_room)
        {
            percent = number_percent();
            if (!can_see(rch, ch))
                percent -= 25; /* Kinda hard to detect someone you can't see. */
            if (rch == ch) /* blah that I forgot this, you don't count. */
                continue;
            if (IS_AWAKE(rch))
            {
                if (IS_AFFECTED(rch, AFF_SIT_AWARE))
                {
                    percent += 15;
                }
            }
            else /* How are you going to see someone give something when you are asleep? */
            {
                percent = 0;
            }
            if (IS_IMMORTAL(rch) && IS_AWAKE(rch)) /* You aren't sneaking anything by an immortal. */
                percent += 100;
            if (percent > chance)
            {
                set_char_color(AT_ACTION, rch);
                if (victim == rch)
                    ch_printf(rch, "%s\n\r", buf);
                else
                    ch_printf(rch, "%s gives %s some credits.\n\r", NAME(ch), NAME(victim));
                learn_from_failure(ch, gsn_slide);
            }
            else
                learn_from_success(ch, gsn_slide);
        }
        send_to_char("OK.\n\r", ch);
        mprog_bribe_trigger(victim, ch, amount);
        if (IS_SET(sysdata.save_flags, SV_GIVE) && !char_died(ch))
            save_char_obj(ch);
        if (IS_SET(sysdata.save_flags, SV_RECEIVE) && !char_died(victim))
            save_char_obj(victim);
        return;
    }

    if ((obj = get_obj_carry(ch, arg1)) == NULL)
    {
        send_to_char("You do not have that item.\n\r", ch);
        return;
    }

    if (obj->wear_loc != WEAR_NONE)
    {
        send_to_char("You must remove it first.\n\r", ch);
        return;
    }

    if ((victim = get_char_room(ch, arg2)) == NULL)
    {
        send_to_char("They aren't here.\n\r", ch);
        return;
    }

    if (!can_drop_obj(ch, obj))
    {
        send_to_char("You can't let go of it.\n\r", ch);
        return;
    }

    if (victim->carry_number + (get_obj_number(obj) / obj->count) > can_carry_n(victim))
    {
        act(AT_PLAIN, "$N has $S hands full.", ch, NULL, victim, TO_CHAR);
        return;
    }

    if (victim->carry_weight + (get_obj_weight(obj) / obj->count) > can_carry_w(victim))
    {
        act(AT_PLAIN, "$N can't carry that much weight.", ch, NULL, victim, TO_CHAR);
        return;
    }

    if (!can_see_obj(victim, obj))
    {
        act(AT_PLAIN, "$N can't see it.", ch, NULL, victim, TO_CHAR);
        return;
    }
    if (IS_OBJ_STAT(obj, ITEM_PROTOTYPE) && !can_take_proto(victim))
    {
        act(AT_PLAIN, "You cannot give that to $N!", ch, NULL, victim, TO_CHAR);
        return;
    }

    separate_obj(obj);
    obj_from_char(obj);
    set_char_color(AT_ACTION, ch);
    ch_printf(ch, "You slide %s to %s.\n\r", obj->short_descr, NAME(victim));
    for (rch = ch->in_room->first_person; rch; rch = rch->next_in_room)
    {
        percent = number_percent();
        if (!can_see(rch, ch))
            percent -= 25; /* Kinda hard to detect someone you can't see. */
        if (rch == ch) /* blah that I forgot this, you don't count. */
            continue;
        if (IS_AWAKE(rch))
        {
            if (IS_AFFECTED(rch, AFF_SIT_AWARE))
            {
                percent += 15;
            }
        }
        else /* How are you going to see someone give something when you are asleep? */
        {
            percent = 0;
        }
        if (IS_IMMORTAL(rch) && IS_AWAKE(rch)) /* You aren't sneaking anything by an immortal. */
            percent += 100;
        if (percent > chance)
        {
            set_char_color(AT_ACTION, rch);
            if (victim == rch)
                ch_printf(rch, "%s gives you %s.\n\r", NAME(ch), obj->short_descr);
            else
                ch_printf(rch, "%s gives %s %s.\n\r", NAME(ch), NAME(victim), obj->short_descr);
            learn_from_failure(ch, gsn_slide);
        }
        else
            learn_from_success(ch, gsn_slide);
    }
    obj = obj_to_char(obj, victim);

    mprog_give_trigger(victim, ch, obj);
    if (IS_SET(sysdata.save_flags, SV_GIVE) && !char_died(ch))
        save_char_obj(ch);
    if (IS_SET(sysdata.save_flags, SV_RECEIVE) && !char_died(victim))
        save_char_obj(victim);
    return;
}


/* Seal- By Arcturus */
void do_seal(CHAR_DATA * ch, const char *argument)
{
    char arg[MAX_INPUT_LENGTH];
    EXIT_DATA* pexit;
    int chance;
    one_argument(argument, arg);

    if (arg[0] == '\0')
    {
        send_to_char("Seal what?\n\r", ch);
        return;
    }
    if ((pexit = find_door(ch, arg, TRUE)) != NULL)
    {
        /* 'lock door' */
        if (IS_SET(pexit->exit_info, EX_SECRET))
        {
            send_to_char("You don't see that here.\n\r", ch); return;
        }
        if (!IS_SET(pexit->exit_info, EX_ISDOOR))
        {
            send_to_char("You can't do that.\n\r", ch); return;
        }
        if (!IS_SET(pexit->exit_info, EX_CLOSED))
        {
            send_to_char("It's not closed.\n\r", ch); return;
        }
        if (pexit->key < 0)
        {
            send_to_char("It can't be sealed.\n\r", ch); return;
        }
        if (IS_SET(pexit->exit_info, EX_LOCKED))
        {
            send_to_char("It's already locked.\n\r", ch); return;
        }
        chance = IS_NPC(ch) ? ch->skill_level[SMUGGLING_ABILITY]
            : ch->pcdata->learned[gsn_seal];
        if (number_percent() > chance)
        {
            send_to_char("You failed.\n\r", ch);
            learn_from_failure(ch, gsn_seal);
            act(AT_ACTION, "$n attempts to seal up the $d.", ch, NULL, pexit->keyword, TO_ROOM);
            return;
        }
        send_to_char("*Clack*\n\r", ch);
        act(AT_ACTION, "$n seals up the $d.", ch, NULL, pexit->keyword, TO_ROOM);
        set_bexit_flag(pexit, EX_LOCKED);
        learn_from_success(ch, gsn_seal);
        return;
    }
    ch_printf(ch, "You see no %s here.\n\r", arg);
    return;
}


/* Written or installed by Raijen, force rank removed by Arcturus */

void gain_honour(CHAR_DATA* ch, int hincrease)
{
    ch->pcdata->status += hincrease;
}

void loose_honour(CHAR_DATA* ch, int hdecrease)
{
    ch->pcdata->status -= hdecrease;
}


