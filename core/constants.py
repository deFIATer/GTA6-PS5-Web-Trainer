"""Constants, PS5Debug-NG opcodes, GTA VI PPSA38500 offsets, and vehicle categories."""

from __future__ import annotations

# ps5debug-NG opcodes
CMD_PROC_WRITE = 0xBDAA0003
CMD_PROC_CALL = 0xBDAA0006
CMD_PROC_PROTECT = 0xBDAA0008
CMD_CONSOLE_NOTIFY = 0xBDDD0004

# PPSA38500 01.002.001 offsets relative to eboot.bin image base (from Offsets.h)
OFF_GOT_PAD_READ_STATE = 0xA3EA780
OFF_IMAGE_SIZE = 0xE7C8000
OFF_CHEAT_TABLE = 0x9ECE1D0
OFF_CHEAT_HEAL_REPAIR = 0x53775C0
OFF_CHEAT_WANTED_UP = 0x5377AC0
OFF_CHEAT_WANTED_DOWN = 0x5377D50
OFF_CHEAT_WEATHER = 0x53780C0

OFF_SPAWN_VEHICLE = 0x5378380  # (u64 modelHash)
OFF_CLOCK_SET_TIME = 0x537AFD0  # (h, m, s)
OFF_GET_PLAYER_COMP = 0x1637050  # (u32 handle) -> comp*
OFF_SET_PLAYER_WANTED = 0x53ACD60  # (ped*, level, 1, 1, 1)
OFF_HEALTH_RESTORE = 0x4F0C3B0  # (u32 handle)
OFF_VEHICLE_FIX = 0x80FEF80  # (vehicle*)
OFF_STREAM_REQUEST = 0x1908520  # (u16* req, u32* model, ctx, 0, 9, 1, 0, aux)
OFF_STREAM_READY_A = 0x693730  # (u16* req) -> bool
OFF_STREAM_READY_B = 0x685DF0  # (u16* req) -> bool
OFF_STREAM_RELEASE = 0x6B9DA0  # (u16* req)

OFF_LOCAL_PLAYER_PED = 0xDCDBCB0  # ped*, handle at +0x9C
OFF_LOCAL_PLAYER_VEH = 0xDCEBCE0  # vehicle*
OFF_GAME_CLOCK = 0xDCEC6D0  # int[]: hour [4], minute [5]
OFF_STREAM_CTX = 0xCC64B30
OFF_ARCHETYPE_MAP = 0xE561800
OFF_LAST_CHEAT_VEH = 0xA753E4C  # u32 handle
OFF_WANTED_LEVEL = 0x5EFC  # in player component

# Control block layout (RW page at ctrl_addr):
#   +0x00 u64 orig_pad    (original scePadReadState pointer)
#   +0x08 u32 state       (0=idle, 1=pending, 2=running, 3=done)
#   +0x0C u32 tick_count  (incremented every game frame)
#   +0x10 u64 fn
#   +0x18 u64 rdi
#   +0x20 u64 rsi
#   +0x28 u64 rdx
#   +0x30 u64 rcx
#   +0x38 u64 r8
#   +0x40 u64 r9
#   +0x48 u64 arg7
#   +0x50 u64 arg8
#   +0x58 u64 result
#   +0x100 scratch area (req, model, aux)



QUICK_VEHICLES = [
    "Banshee",
    "APC",
    "ambulance",
    "airbus",
    "baller8",
    "bandito",
    "sanchez",
    "buzzard",
]

VEHICLE_CATEGORIES: dict[str, set[str]] = {
    "Supersamochody": {
        "cheetah2", "coquette4", "emerus", "furia", "infernus2", "italirsx",
        "lasso", "mambagt", "ragno", "tempesta", "thrax", "vitello", "wymankitcar", "zorrusso",
    },
    "Sportowe": {
        "banshee", "blista2", "buffalo", "buffalo6", "carbonizzare", "comet7", "cometretro",
        "coquette", "coquette2", "cypher", "drafter", "eboost", "elegy", "fr36", "futo",
        "growler", "jugular", "neon", "paragon", "penumbra", "penumbraff", "raiden",
        "schafter3", "sentinelclassic2", "sugoi", "sultan", "sungrazer", "tailgater2",
        "transgressor", "vectre", "vstr", "zion2",
    },
    "Muscle": {
        "buccaneer", "chino", "crusher", "deviant", "dominator", "dominator7", "dominator9",
        "dominatorclassic", "gauntlet4", "gauntlet5", "hermes", "hustler", "impaler5",
        "impaler6", "lowline", "manana", "phoenix", "picador", "ruiner", "slamvan",
        "stallion", "tulip", "tulip2", "vamos", "vigero3", "yosemite", "yosemite1500",
    },
    "Sedany i Coupe": {
        "animus", "asterope2", "bogan", "chavosxs", "declasse500", "emperor", "feroci",
        "grouse", "intruder", "meritlx", "pmp700", "primo", "schafters", "stanier",
        "stanier55", "stanierg2", "vivanite", "willard", "windsor2",
    },
    "Kompakty": {
        "akane", "aphid", "crake", "devanity", "erode", "greta", "iota", "moocher",
        "nimbe", "parkland", "ridgway", "schutzen", "shifter", "trotter", "woodland",
        "216cstreamer", "arcturus",
    },
    "SUV i Kombi": {
        "aleutian", "allander", "astron", "baller2", "baller8", "cavalcade3", "contender",
        "dorado", "dubsta", "granger", "granger3600lx", "gresleyxt", "jubilee",
        "landstalker2", "mesa", "montagne", "montagnepu", "montagnexl", "patriot3",
        "reblagts", "toros", "xls",
    },
    "Terenowe i Pickupy": {
        "bandito", "bfinjection4", "bison", "bison1800", "bison3800", "bison4", "bison900",
        "blast", "bobcatxl", "caracara4x4", "crawler", "dominatorbuggy", "ganado", "hellion",
        "kamacho", "marshall", "outlaw", "rancher", "rebel", "reclaimer", "reclaimerclassic",
        "riata", "riataclassic", "sandking_4dr", "sandkingxt", "walton",
    },
    "Motocykle": {
        "alvinov1", "avarus", "blazer4", "carbonrs", "cliffhanger", "double", "dzy200",
        "dzy80", "enduro", "esskey", "fcr", "lectro", "manchez", "mb210", "nightblade2",
        "pulsebike", "sanchez", "sfb1000", "sovereign107", "verus", "zombieb",
    },
    "Rowery": {
        "bmx", "cruiser", "lombike", "mobilityscooter", "scorcher", "tribike", "wheelchair",
    },
    "Samoloty": {
        "duster", "duster2", "jet", "jet2", "lazer", "mallard", "mammatus", "nimbus", "shamal",
    },
    "Helikoptery": {
        "annihilator", "buzzard", "cargobob", "maverick", "polmav", "supervolito",
        "swift", "valkyrie", "volatus",
    },
    "Służby i Wojskowe": {
        "ambulance", "ambulance2", "apc", "firetruk2", "guardiantx2",
        "rhino", "riot", "stockade", "wastelander",
    },
    "Łodzie": {
        "jetmax", "swamp",
    },
    "Vany i Ciężarówki": {
        "airbus", "airtug", "benson", "biff", "boxville", "brigade", "bulldozer",
        "burrito", "bus2", "bus2_dmg", "bus2_frag", "caddy", "caddy2", "coach",
        "crane", "digger", "dump", "forklift", "furrowtc2350", "golfcarthotrod",
        "hauler", "journey", "minivan", "mule", "packer", "phantom3", "rentalbus",
        "ripley", "rubble", "rumpo", "shuttle", "speedo", "spritz", "taco",
        "telehandler", "tiptruck", "towtruck", "tractor2", "youga2", "yougaxl",
    },
}

CATEGORY_ORDER = [
    "Wszystkie",
    "Supersamochody",
    "Sportowe",
    "Muscle",
    "Sedany i Coupe",
    "Kompakty",
    "SUV i Kombi",
    "Terenowe i Pickupy",
    "Motocykle",
    "Rowery",
    "Samoloty",
    "Helikoptery",
    "Służby i Wojskowe",
    "Łodzie",
    "Vany i Ciężarówki",
    "Inne",
]


