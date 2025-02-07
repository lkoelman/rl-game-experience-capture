"""
Path of Exile 2 game state and actions.
"""
from dataclasses import dataclass
from enum import IntEnum, StrEnum
from typing import List, Optional, TypeAlias, Union

@dataclass
class CharacterInfo:
    character_class: str
    level: int
    max_health: int
    max_mana: int
    max_energy_shield: int

@dataclass
class PlayerStats:
    health: int
    max_health: int
    mana: int
    max_mana: int
    energy_shield: int
    max_energy_shield: int
    debuffs: List[str]  # List of active debuff names

    # TODO: character damage and loadout

# @dataclass
# class InventoryState:
#     is_open: bool
#     fullness_percent: float  # 0.0 to 1.0
#     is_full: bool

class UiStatus(IntEnum):
    """
    Mutually exclusive UI modes.
    I.e. are we in a menu, playing the game, ...?
    """
    # UI elements when game is played using gamepad
    UI_GAMEPAD_PLAYING = 1
    UI_GAMEPAD_DEATH = 2                # player has died
    UI_GAMEPAD_INVENTORY_SCREEN = 3

# @dataclass
# class UIState:
#     is_menu_open: bool
#     inventory: InventoryState

class WorldLocation(IntEnum):
    ACT1_RIVERBANK = 1
    ACT1_CLEARFELL = 2

@dataclass
class LocationInfo:
    is_in_safe_zone: bool
    zone_name: Optional[str] = None

@dataclass
class GameState:
    character: CharacterInfo
    player: PlayerStats
    ui: UiStatus
    location: LocationInfo

##################################
# Controls

class SkillAction(IntEnum):
    # Totem Skills
    ANCESTRAL_WARRIOR_TOTEM = 1
    ARTILLERY_BALLISTA = 2
    RIPWIRE_BALLISTA = 3
    SHOCKWAVE_TOTEM = 4

    # Melee Skills
    ARMOUR_BREAKER = 10
    BONESHATTER = 11
    EARTHQUAKE = 12
    EARTHSHATTER = 13
    FURIOUS_SLAM = 14
    HAMMER_OF_THE_GODS = 15
    HAND_OF_CHAYULA = 16
    KILLING_PALM = 17
    LEAP_SLAM = 18
    MACE_STRIKE = 19
    MAUL = 20
    PERFECT_STRIKE = 21
    RAPID_ASSAULT = 22
    ROLLING_SLAM = 23
    SHATTERING_PALM = 24
    SIPHONING_STRIKE = 25
    STAGGERING_PALM = 26
    SUNDER = 27
    SUPERCHARGED_SLAM = 28
    VAULTING_IMPACT = 29
    VOLCANIC_FISSURE = 30
    WHIRLING_ASSAULT = 31
    WIND_BLAST = 32

    # Bow Skills
    BOW_SHOT = 40
    CROSSBOW_SHOT = 41
    DETONATING_ARROW = 42
    ELECTROCUTING_ARROW = 43
    ESCAPE_SHOT = 44
    GAS_ARROW = 45
    ICE_SHOT = 46
    LIGHTNING_ARROW = 47
    POISONBURST_ARROW = 48
    RAIN_OF_ARROWS = 49
    SHOCKCHAIN_ARROW = 50
    SNIPE = 51
    SPIRAL_VOLLEY = 52
    STORMCALLER_ARROW = 53
    TORNADO_SHOT = 54
    TOXIC_GROWTH = 55
    VINE_ARROW = 56

    # Spear Skills
    BLAZING_LANCE = 70
    ICE_SPEAR = 71
    IMPALE = 72
    PUNCTURE = 73
    SPEARFIELD = 74
    STORM_SPEAR = 75

    # Quarterstaff Skills
    FROZEN_LOCUS = 90
    GATHERING_STORM = 91
    GLACIAL_CASCADE = 92
    STORM_WAVE = 93
    TEMPEST_BELL = 94
    TEMPEST_FLURRY = 95
    WAVE_OF_FROST = 96
    WHIRLING_SLASH = 97

    # Crossbow Skills
    CLUSTER_GRENADE = 110
    EXPLOSIVE_GRENADE = 111
    EXPLOSIVE_SHOT = 112
    FLASH_GRENADE = 113
    GAS_GRENADE = 114
    HAILSTORM_ROUNDS = 115
    OIL_GRENADE = 116
    PLASMA_BLAST = 117
    SHOCKBURST_ROUNDS = 118
    SIEGE_CASCADE = 119
    VOLTAIC_GRENADE = 120

    # Elemental Skills
    BLEEDING_CONCOCTION = 140
    FLICKER_STRIKE = 141
    FREEZING_SALVO = 142
    ICE_STRIKE = 143
    MAGNETIC_SALVO = 144
    SHIELD_CHARGE = 145
    SHIELD_WALL = 146
    VOLTAIC_MARK = 147

    # Misc Skills
    SANDSTORM_SWIPE = 160
    STAMPEDE = 161
    RESONATING_SHIELD = 162
    MAGMA_BARRIER = 163
    # SHIELD_CHARGE = 164
    # VOLTAIC_MARK = 165

    # ========================================
    # Spell Skills
    ARC = 200
    BALL_LIGHTNING = 201
    BARRAGE = 202
    BONE_BLAST = 203
    BONE_CAGE = 204
    BONE_OFFERING = 205
    BONESTORM = 206
    CHAOS_BOLT = 207
    CHARGED_STAFF = 208
    COLD_SNAP = 209
    COMET = 210
    CONSECRATE = 211
    CONDUCTIVITY = 212
    CONTAGION = 213
    DARK_EFFIGY = 214
    DECOMPOSE = 215
    DESPAIR = 216
    DETONATE_DEAD = 217
    EMBER_FUSILLADE = 218
    ENFEEBLE = 219
    ESSENCE_DRAIN = 220
    EYE_OF_WINTER = 221
    EXSANGUINATE = 222
    FIREBALL = 223
    FIREBOLT = 224
    FIRESTORM = 225
    FLAME_WALL = 226
    FLAMEBLAST = 227
    FLAMMABILITY = 228
    FREEZING_MARK = 229
    FREEZING_SHARDS = 230
    FROST_BOMB = 231
    FROST_WALL = 232
    FROSTBOLT = 233
    GALVANIC_FIELD = 234
    HEXBLAST = 235
    HYPOTHERMIA = 236
    ICE_NOVA = 237
    INCINERATE = 238
    LIGHTNING_BOLT = 239
    LIGHTNING_CONDUIT = 240
    LIGHTNING_STORM = 241
    LIGHTNING_WARP = 242
    LIVING_BOMB = 243
    MANA_DRAIN = 244
    MANA_TEMPEST = 245
    ORB_OF_STORMS = 246
    POWER_SIPHON = 247
    PROFANE_RITUAL = 248
    RAISE_SHIELD = 249
    REAP = 250
    ROLLING_MAGMA = 251
    SIGIL_OF_POWER = 252
    SNIPERS_MARK = 253
    SOLAR_ORB = 254
    SPARK = 255
    SOULREND = 256
    SUMMON_WOLF = 257
    TEMPORAL_CHAINS = 258
    UNEARTH = 259
    VOLATILE_DEAD = 260
    VOLTAIC_MARK = 261
    VULNERABILITY = 262
    WITHER = 263

    # ==========================================
    # Skill Gem : Quarterstaff
    FALLING_THUNDER = 300
    GLACIAL_CASCADE = 301
    KILLING_PALM = 302
    FROZEN_LOCUS = 303
    STAGGERING_PALM = 304
    TEMPEST_BELL = 305
    VAULTING_IMPACT = 306
    WIND_BLAST = 307
    ICE_STRIKE = 308
    TEMPEST_FLURRY = 309
    WAVE_OF_FROST = 310
    SIPHONING_STRIKE = 311
    STORM_WAVE = 312
    FREEZING_MARK = 313
    CHARGED_STAFF = 314
    MANTRA_OF_DESTRUCTION = 315
    HAND_OF_CHAYULA = 316
    WHIRLING_ASSAULT = 317
    SHATTERING_PALM = 318
    FLICKER_STRIKE = 319

    # ==========================================
    # Skill Gem : Elemental
    SPARK = 350
    ICE_NOVA = 351
    FROST_BOMB = 352
    FLAME_WALL = 353
    ORB_OF_STORMS = 354
    FROSTBOLT = 355
    EMBER_FUSILLADE = 356
    ARC = 357
    INCINERATE = 358
    SOLAR_ORB = 359
    COLD_SNAP = 360
    FLAMMABILITY = 361
    HYPOTHERMIA = 362
    CONDUCTIVITY = 363
    FROST_WALL = 364
    LIGHTNING_WARP = 365
    MANA_TEMPEST = 366
    FIREBALL = 367
    FIRESTORM = 368
    BALL_LIGHTNING = 369
    COMET = 370
    TEMPORAL_CHAINS = 371
    FLAMEBLAST = 372
    EYE_OF_WINTER = 373
    LIGHTNING_CONDUIT = 374

class UiAction(IntEnum):
    # TODO: all UI interactions made through keypresses on gamepad or keyboard
    pass

GameAction: TypeAlias = Union[SkillAction, UiAction]

@dataclass
class GamepadBindings:
    # TODO: map gamepad buttons (see the SDL3 gamepad button docs), create fields for each button AND each allowed combinaton button (left trigger + button)
    pass
