// Плавное затемнение
// для San Fierro Stories - Плавное затемнение 2025 (с) by Mateusz
// ==>

new Text:Black_Background[2];//Черные полосы сверху и снизу

#define BACKGROUND_INTERVAL (25)

#define MAX_BACKGROUND_NAME (31)

#define SIZE_BACKGROUND_NAME (MAX_BACKGROUND_NAME+1)

#define OnBackground%1(playerid) forward %1(playerid, __bg_status); public %1(playerid, __bg_status)

#define GetBackgroundStatus() (__bg_status)

forward _BG_OnBackgroundShow(playerid);
forward _BG_OnBackgroundHide(playerid);

static const _BG_COLORS[] =
{
    0x00000010,
    0x0000001F,
    0x00000020,
    0x0000002F,
    0x00000030,
    0x0000003F,
    0x00000040,
    0x0000004F,
    0x00000050,
    0x0000005F,
    0x00000060,
    0x0000006F,
    0x00000070,
    0x0000007F,
    0x00000080,
    0x0000008F,
    0x00000090,
    0x0000009F,
    0x000000A0,
    0x000000AF,
    0x000000B0,
    0x000000BF,
    0x000000C0,
    0x000000CF,
    0x000000D0,
    0x000000DF,
    0x000000E0,
    0x000000EF,
    0x000000F0,
    0x000000FF
};

static const _BG_COLORS_COUNT = (sizeof(_BG_COLORS)-1);

enum
{
    BACKGROUND_STATUS_INVALID,

    BACKGROUND_STATUS_BEGIN,
    BACKGROUND_STATUS_SHADING,
    BACKGROUND_STATUS_SHADED,
    BACKGROUND_STATUS_LIGHTING,
    BACKGROUND_STATUS_END
}

static stock PlayerText:_bg_ptd[MAX_PLAYERS];

static stock bool:_bg_mode[MAX_PLAYERS],

             _bg_status[MAX_PLAYERS],
             _bg_color[MAX_PLAYERS],
             _bg_pause[MAX_PLAYERS],

             _bg_timer[MAX_PLAYERS],

             _bg_function[MAX_PLAYERS][SIZE_BACKGROUND_NAME];


stock TogglePlayerBackground(playerid)
{
    if(_bg_mode[playerid])
    {
        return 0;
    }

    new pause=1800;

	new function[]="", string[]="LD_SPAC:white";

    _bg_mode[playerid] = true;
    _bg_status[playerid] = BACKGROUND_STATUS_BEGIN;


    if(pause < BACKGROUND_INTERVAL)
        pause = BACKGROUND_INTERVAL;

    _bg_pause[playerid] = pause;


    if(function[0] != EOS)
    {
        _bg_function[playerid][0] = EOS;
        strcat(_bg_function[playerid], function);


        CallLocalFunction(function, "ii", playerid, _bg_status[playerid]);
    }


    _bg_ptd[playerid] = CreatePlayerTextDraw(playerid, -1.0, -1.0, string);

    PlayerTextDrawTextSize(playerid, _bg_ptd[playerid], (640.0+2.0), (448.0+2.0));
    PlayerTextDrawSetSelectable(playerid, _bg_ptd[playerid], false);
    PlayerTextDrawColor(playerid, _bg_ptd[playerid], 0x00000000);
    PlayerTextDrawFont(playerid, _bg_ptd[playerid], TEXT_DRAW_FONT_SPRITE_DRAW);

    PlayerTextDrawShow(playerid, _bg_ptd[playerid]);


    return _bg_timer[playerid] = SetTimerEx("_BG_OnBackgroundShow", BACKGROUND_INTERVAL, false, "i", playerid);
}



public _BG_OnBackgroundShow(playerid)
{
    if(_bg_color[playerid] < _BG_COLORS_COUNT)
    {
        if(_bg_color[playerid] == 0)
        {
            _bg_status[playerid] = BACKGROUND_STATUS_SHADING;


            if(_bg_function[playerid][0] != EOS)
            {
                CallLocalFunction(_bg_function[playerid], "ii", playerid, _bg_status[playerid]);
            }
        }


        _bg_color[playerid]++;


        PlayerTextDrawColor(playerid, _bg_ptd[playerid], _BG_COLORS[_bg_color[playerid]]);
        PlayerTextDrawShow(playerid, _bg_ptd[playerid]);


        return _bg_timer[playerid] = SetTimerEx("_BG_OnBackgroundShow", BACKGROUND_INTERVAL, false, "i", playerid);
    }
    else
    {
        _bg_status[playerid] = BACKGROUND_STATUS_SHADED;


        if(_bg_function[playerid][0] != EOS)
        {
            CallLocalFunction(_bg_function[playerid], "ii", playerid, _bg_status[playerid]);
        }


        return _bg_timer[playerid] = SetTimerEx("_BG_OnBackgroundHide", _bg_pause[playerid], false, "i", playerid);
    }
}


public _BG_OnBackgroundHide(playerid)
{
    if(_bg_color[playerid] > 0)
    {
        if(_bg_color[playerid] == _BG_COLORS_COUNT)
        {
            _bg_status[playerid] = BACKGROUND_STATUS_LIGHTING;


            if(_bg_function[playerid][0] != EOS)
            {
                CallLocalFunction(_bg_function[playerid], "ii", playerid, _bg_status[playerid]);
            }
        }

        _bg_color[playerid]--;

        PlayerTextDrawColor(playerid, _bg_ptd[playerid], _BG_COLORS[_bg_color[playerid]]);
        PlayerTextDrawShow(playerid, _bg_ptd[playerid]);

        return _bg_timer[playerid] = SetTimerEx("_BG_OnBackgroundHide", BACKGROUND_INTERVAL, false, "i", playerid);
    }
    else
    {
        _bg_status[playerid] = BACKGROUND_STATUS_END;

        if(_bg_function[playerid][0] != EOS)
        {
            CallLocalFunction(_bg_function[playerid], "ii", playerid, _bg_status[playerid]);
        }

        PlayerTextDrawDestroy(playerid, _bg_ptd[playerid]);
        return _BG_Clear(playerid);
    }
}

stock _BG_Clear(playerid)
{
	KillTimer(_bg_timer[playerid]);
    _bg_ptd[playerid] = PlayerText:INVALID_TEXT_DRAW;
    _bg_mode[playerid] = false;
    _bg_status[playerid] = BACKGROUND_STATUS_INVALID;
    _bg_color[playerid] = 0;
    _bg_pause[playerid] = 0;
    _bg_timer[playerid] = 0;
    _bg_function[playerid][0] = EOS;
    return 1;
}
