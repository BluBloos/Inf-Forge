#include <automata_engine.hpp>

namespace automata_engine {
    const char *gameKeyToString(game_key_t keyIdx)
    {
        switch (keyIdx) {
            case GAME_KEY_0:
                return "GAME_KEY_0";
            case GAME_KEY_1:
                return "GAME_KEY_1";
            case GAME_KEY_2:
                return "GAME_KEY_2";
            case GAME_KEY_3:
                return "GAME_KEY_3";
            case GAME_KEY_4:
                return "GAME_KEY_4";
            case GAME_KEY_5:
                return "GAME_KEY_5";
            case GAME_KEY_6:
                return "GAME_KEY_6";
            case GAME_KEY_7:
                return "GAME_KEY_7";
            case GAME_KEY_8:
                return "GAME_KEY_8";
            case GAME_KEY_9:
                return "GAME_KEY_9";
            case GAME_KEY_A:
                return "GAME_KEY_A";
            case GAME_KEY_B:
                return "GAME_KEY_B";
            case GAME_KEY_C:
                return "GAME_KEY_C";
            case GAME_KEY_D:
                return "GAME_KEY_D";
            case GAME_KEY_E:
                return "GAME_KEY_E";
            case GAME_KEY_F:
                return "GAME_KEY_F";
            case GAME_KEY_G:
                return "GAME_KEY_G";
            case GAME_KEY_H:
                return "GAME_KEY_H";
            case GAME_KEY_I:
                return "GAME_KEY_I";
            case GAME_KEY_J:
                return "GAME_KEY_J";
            case GAME_KEY_K:
                return "GAME_KEY_K";
            case GAME_KEY_L:
                return "GAME_KEY_L";
            case GAME_KEY_M:
                return "GAME_KEY_M";
            case GAME_KEY_N:
                return "GAME_KEY_N";
            case GAME_KEY_O:
                return "GAME_KEY_O";
            case GAME_KEY_P:
                return "GAME_KEY_P";
            case GAME_KEY_Q:
                return "GAME_KEY_Q";
            case GAME_KEY_R:
                return "GAME_KEY_R";
            case GAME_KEY_S:
                return "GAME_KEY_S";
            case GAME_KEY_T:
                return "GAME_KEY_T";
            case GAME_KEY_U:
                return "GAME_KEY_U";
            case GAME_KEY_V:
                return "GAME_KEY_V";
            case GAME_KEY_W:
                return "GAME_KEY_W";
            case GAME_KEY_X:
                return "GAME_KEY_X";
            case GAME_KEY_Y:
                return "GAME_KEY_Y";
            case GAME_KEY_Z:
                return "GAME_KEY_Z";
            case GAME_KEY_SHIFT:
                return "GAME_KEY_SHIFT";
            case GAME_KEY_SPACE:
                return "GAME_KEY_SPACE";
            case GAME_KEY_ESCAPE:
                return "GAME_KEY_ESCAPE";
            case GAME_KEY_TAB:
                return "GAME_KEY_TAB";
            case GAME_KEY_F5:
                return "GAME_KEY_F5";
        }
        return "<unknown GAME_KEY>";
    }
}  // namespace automata_engine