// written by goldfish :)
// adapted for l4d2_mako_entwatch

#if defined _smHud_included
 #endinput
#endif
#define _smHud_included

#define HUD_FLAG_PRESTR           (1<<0)  // do you want a string/value pair to start(pre) or end(post) with the static string (default is PRE)
#define HUD_FLAG_POSTSTR          (1<<1)  // ditto
#define HUD_FLAG_BEEP             (1<<2)  // Makes a countdown timer blink
#define HUD_FLAG_BLINK            (1<<3)  // do you want this field to be blinking
#define HUD_FLAG_AS_TIME          (1<<4)  // to do..
#define HUD_FLAG_COUNTDOWN_WARN   (1<<5)  // auto blink when the timer gets under 10 seconds
#define HUD_FLAG_NOBG             (1<<6)  // dont draw the background box for this UI element
#define HUD_FLAG_ALLOWNEGTIMER    (1<<7)  // by default Timers stop on 0:00 to avoid briefly going negative over network, this keeps that from happening
#define HUD_FLAG_ALIGN_LEFT       (1<<8)  // Left justify this text
#define HUD_FLAG_ALIGN_CENTER     (1<<9)  // Center justify this text
#define HUD_FLAG_ALIGN_RIGHT      (3<<8)  // Right justify this text
#define HUD_FLAG_TEAM_SURVIVORS   (1<<10) // only show to the survivor team
#define HUD_FLAG_TEAM_INFECTED    (1<<11) // only show to the special infected team
#define HUD_FLAG_TEAM_MASK        (3<<10) // link HUD_FLAG_TEAM_SURVIVORS and HUD_FLAG_TEAM_INFECTED
#define HUD_FLAG_UNKNOWN1         (1<<12) // ?
#define HUD_FLAG_TEXT             (1<<13) // ?
#define HUD_FLAG_NOTVISIBLE       (1<<14) // if you want to keep the slot data but keep it from displaying

#define HUD_LEFT_TOP    0
#define HUD_LEFT_BOT    1
#define HUD_MID_TOP     2
#define HUD_MID_BOT     3
#define HUD_RIGHT_TOP   4
#define HUD_RIGHT_BOT   5
#define HUD_TICKER      6
#define HUD_FAR_LEFT    7
#define HUD_FAR_RIGHT   8
#define HUD_MID_BOX     9
#define HUD_SCORE_TITLE 10
#define HUD_SCORE_1     11
#define HUD_SCORE_2     12
#define HUD_SCORE_3     13
#define HUD_SCORE_4     14

#define TIMER_ID_0 0
#define TIMER_ID_1 1
#define TIMER_ID_2 2
#define TIMER_ID_3 3

#define HUD_SIZE_AUTO   0.0
#define HUD_WIDTH_AUTO  -1.0
#define HUD_WIDTH_MIN   -2.0
#define HUD_WIDTH_MED   -3.0
#define HUD_WIDTH_BIG   -4.0
#define HUD_WIDTH_HUGE  -5.0
#define HUD_WIDTH_MAX   -6.0

#define HUD_HEIGHT_AUTO -1.0
#define HUD_HEIGHT_MIN  -2.0 
#define HUD_HEIGHT_MED  -3.0
#define HUD_HEIGHT_BIG  -4.0 
#define HUD_HEIGHT_HUGE -5.0 
#define HUD_HEIGHT_MAX  -6.0
#define HUD_HEIGHT_1LINE    -7.0
#define HUD_HEIGHT_2LINE    -8.0
#define HUD_HEIGHT_3LINE    -9.0
#define HUD_HEIGHT_4LINE    -10.0

#define TIMER_DISABLE    0
#define TIMER_COUNTUP    1
#define TIMER_COUNTDOWN  2
#define TIMER_STOP       3
#define TIMER_SET        4

#define HUD_ANIM_UNROLL_Y  1
#define HUD_ANIM_ROLLUP_Y  2
#define HUD_ANIM_UNROLL_X  3
#define HUD_ANIM_ROLLUP_X  4
#define HUD_ANIM_BLINK     5

enum struct ScriptedHUD {
    bool isInit;
    bool tableCreated;
    char sVScriptCode[512];
    
    float anm_fWidth[15];
    float anm_fHeight[15];
    float anm_fPos_x[15];
    float anm_fPos_y[15];
    int anm_animID[15];
    int anm_iFlags[15];

    any anm_Params_0[15];
    any anm_Params_1[15];
    any anm_Params_2[15];
    any anm_Params_3[15];
    
    void SetAnimParams(int iSlot, any p1, any p2 = 0, any p3 = 0, any p4 = 0) {
        this.anm_Params_0[iSlot] = p1;
        this.anm_Params_1[iSlot] = p2;
        this.anm_Params_2[iSlot] = p3;
        this.anm_Params_3[iSlot] = p4;
    }
    
    void Init() {
        this.isInit = true;
    }

    void AddNewHUD_asTEXT(int iSlot, char [] sText, int iFlags = 0) {
        if (iSlot > 14 || iSlot < 0) return;
        if (!this.tableCreated) {
            this.tableCreated = true;
            L4D2_RunScript("if (!(\"MyHUD\" in getroottable())) ::MyHUD <- { Fields = {} };");
            this.Init();
        }
        Format(this.sVScriptCode, sizeof(this.sVScriptCode), "::MyHUD.Fields.t%d <- { slot = %d, dataval = \"%s\", flags = %d };", iSlot, iSlot, sText, iFlags);
        L4D2_RunScript(this.sVScriptCode);
    }
    
    void AddNewHUD_asTIMER(int iSlot, char [] sText, int iStartTime, int iFlags = 0, int iTimerID = 0, int iTimerMode = TIMER_COUNTDOWN) {
        if (iSlot > 14 || iSlot < 0) return;
        if (!this.tableCreated) {
            this.tableCreated = true;
            L4D2_RunScript("if (!(\"MyHUD\" in getroottable())) ::MyHUD <- { Fields = {} };");
        }
    
        iTimerID = (iTimerID > 3) ? 3 : iTimerID;
        int iSpecial = iTimerID;
        Format(this.sVScriptCode, sizeof(this.sVScriptCode), "::MyHUD.Fields.t%d <- {slot=%d, staticstring=\"%s\", special=%d, flags=%d}; HUDManageTimers(%d,%d,%d);", iSlot, iSlot, sText, iSpecial, iFlags, iTimerID, iTimerMode, iStartTime);
        L4D2_RunScript(this.sVScriptCode);
    }
    
    void DeleteAllSlots() {
        L4D2_RunScript("::MyHUD<-{Fields={}};HUDManageTimers(0,3,0); HUDManageTimers(1,3,0); HUDManageTimers(2,3,0); HUDManageTimers(3,3,0);HUDSetLayout(::MyHUD);");
    } 
    
    void Place_CenteredX(int iSlot, float fWidth = HUD_SIZE_AUTO, float fHeight = HUD_HEIGHT_AUTO, float fPos_y = 0.1, int animID = 0) {
        float fPos_x;
        fPos_x = GetCenteredX(fWidth);
        this.Place(iSlot, fWidth, fHeight, fPos_x, fPos_y, animID);
    }
    
    void Place(int iSlot, float fWidth = HUD_SIZE_AUTO, float fHeight = HUD_HEIGHT_AUTO, float fPos_x = 0.1, float fPos_y = 0.1, int animID = 0) {
        if (iSlot > 14 || iSlot < 0) return;

        this.getHudParam(fWidth, fHeight);
        if (fPos_x < 0.0 || fPos_x > 1.0) fPos_x = 0.05;
        if (fPos_y < 0.0 || fPos_y > 1.0) fPos_y = 0.05;
        
        this.anm_fHeight[iSlot] = fHeight;
        this.anm_fWidth[iSlot] = fWidth;
        this.anm_fPos_x[iSlot] = fPos_x;
        this.anm_fPos_y[iSlot] = fPos_y;
        
        if (animID > 0) this.anm_animID[iSlot] = animID;
        Format(this.sVScriptCode, sizeof(this.sVScriptCode), "HUDPlace(%d, %f, %f, %f, %f);", iSlot, fPos_x, fPos_y, fWidth, fHeight);
        L4D2_RunScript(this.sVScriptCode);
    }
    
    void getHudParam(float &_fWidth, float &_fHeight) {
        bool isAuto;
        if (_fWidth <= 0.0) {
            switch (_fWidth) {
                case HUD_SIZE_AUTO: isAuto = true;
                case HUD_WIDTH_AUTO: _fWidth = 0.15;
                case HUD_WIDTH_MIN:  _fWidth = 0.15;
                case HUD_WIDTH_MED:  _fWidth = 0.25;
                case HUD_WIDTH_BIG:  _fWidth = 0.35;
                case HUD_WIDTH_HUGE: _fWidth = 0.5;
                case HUD_WIDTH_MAX:  _fWidth = 1.0;
            }
        }
        if (_fHeight <= 0.0) {
            if (isAuto) _fHeight = HUD_HEIGHT_AUTO;
            switch (_fHeight) {
                case HUD_HEIGHT_1LINE: _fHeight = 0.028;
                case HUD_HEIGHT_2LINE: _fHeight = 0.056;
                case HUD_HEIGHT_3LINE: _fHeight = 0.084;
                case HUD_HEIGHT_4LINE: _fHeight = 0.112;
                case HUD_HEIGHT_AUTO,HUD_SIZE_AUTO: _fHeight = 0.06;
                case HUD_HEIGHT_MIN:  _fHeight = 0.045;
                case HUD_HEIGHT_MED:  _fHeight = 0.13;
                case HUD_HEIGHT_BIG:  _fHeight = 0.3;
                case HUD_HEIGHT_HUGE: _fHeight = 0.6;
                case HUD_HEIGHT_MAX:  _fHeight = 1.0;
            }
        }
    }

    void Draw() {
        GameRules_SetProp("m_bChallengeModeActive", 1);
        L4D2_RunScript("HUDSetLayout(::MyHUD);");
    }
}

stock void L4D2_RunScript(const char[] sCode) {
    static int s_iScriptLogic = INVALID_ENT_REFERENCE;
    int entity = EntRefToEntIndex(s_iScriptLogic);
    if (entity == INVALID_ENT_REFERENCE || !IsValidEntity(entity)) {
        entity = CreateEntityByName("logic_script");
        if (entity == -1 || !IsValidEntity(entity)) {
            LogError("Could not create 'logic_script'");
            return;
        }
        DispatchSpawn(entity);
        s_iScriptLogic = EntIndexToEntRef(entity);
    }
    SetVariantString(sCode);
    AcceptEntityInput(entity, "RunScriptCode");
}

stock float GetCenteredX(float width) {
    return 0.5 - (width / 2.0);
}

ScriptedHUD HUD;
