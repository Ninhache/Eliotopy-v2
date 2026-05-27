#pragma once
#include <cstdint>

enum class BreedEnum : int32_t {
    Undefined = 0,
    Feca = 1,
    Osamodas = 2,
    Enutrof = 3,
    Sram = 4,
    Xelor = 5,
    Ecaflip = 6,
    Eniripsa = 7,
    Iop = 8,
    Cra = 9,
    Sadida = 10,
    Sacrieur = 11,
    Pandawa = 12,
    Roublard = 13,
    Zobal = 14,
    Steamer = 15,
    Eliotrope = 16,
    Huppermage = 17,
    Ouginak = 18,
    Forgelance = 19,
    Summoned = 20,
    Monster = 21,
    MonsterGroup = 22,
    NPC = 23,
    TaxCollector = 24,
    Mutant = 25,
    MutantInDungeon = 26,
    MountOutside = 27,
    Prism = 28
};

inline const char* toString(BreedEnum b) {
    switch (b) {
        case BreedEnum::Feca:            return "Feca";
        case BreedEnum::Osamodas:        return "Osamodas";
        case BreedEnum::Enutrof:         return "Enutrof";
        case BreedEnum::Sram:            return "Sram";
        case BreedEnum::Xelor:           return "Xelor";
        case BreedEnum::Ecaflip:         return "Ecaflip";
        case BreedEnum::Eniripsa:        return "Eniripsa";
        case BreedEnum::Iop:             return "Iop";
        case BreedEnum::Cra:             return "Cra";
        case BreedEnum::Sadida:          return "Sadida";
        case BreedEnum::Sacrieur:        return "Sacrieur";
        case BreedEnum::Pandawa:         return "Pandawa";
        case BreedEnum::Roublard:        return "Roublard";
        case BreedEnum::Zobal:           return "Zobal";
        case BreedEnum::Steamer:         return "Steamer";
        case BreedEnum::Eliotrope:       return "Eliotrope";
        case BreedEnum::Huppermage:      return "Huppermage";
        case BreedEnum::Ouginak:         return "Ouginak";
        case BreedEnum::Forgelance:      return "Forgelance";
        default:                         return "Unknown";
    }
}