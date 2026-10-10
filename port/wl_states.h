/* Auto-generated state declarations and table */
#ifndef WL_STATES_H
#define WL_STATES_H

#ifndef SPEAR
#define WL_NUM_STATES 305

#ifdef WL_STATES_DEFINE
statetype s_player;
statetype s_attack;
statetype s_rocket;
statetype s_smoke1;
statetype s_smoke2;
statetype s_smoke3;
statetype s_smoke4;
statetype s_boom1;
statetype s_boom2;
statetype s_boom3;
statetype s_grdstand;
statetype s_grdpath1;
statetype s_grdpath1s;
statetype s_grdpath2;
statetype s_grdpath3;
statetype s_grdpath3s;
statetype s_grdpath4;
statetype s_grdpain;
statetype s_grdpain1;
statetype s_grdshoot1;
statetype s_grdshoot2;
statetype s_grdshoot3;
statetype s_grdchase1;
statetype s_grdchase1s;
statetype s_grdchase2;
statetype s_grdchase3;
statetype s_grdchase3s;
statetype s_grdchase4;
statetype s_grddie1;
statetype s_grddie2;
statetype s_grddie3;
statetype s_grddie4;
statetype s_blinkychase1;
statetype s_blinkychase2;
statetype s_inkychase1;
statetype s_inkychase2;
statetype s_pinkychase1;
statetype s_pinkychase2;
statetype s_clydechase1;
statetype s_clydechase2;
statetype s_dogpath1;
statetype s_dogpath1s;
statetype s_dogpath2;
statetype s_dogpath3;
statetype s_dogpath3s;
statetype s_dogpath4;
statetype s_dogjump1;
statetype s_dogjump2;
statetype s_dogjump3;
statetype s_dogjump4;
statetype s_dogjump5;
statetype s_dogchase1;
statetype s_dogchase1s;
statetype s_dogchase2;
statetype s_dogchase3;
statetype s_dogchase3s;
statetype s_dogchase4;
statetype s_dogdie1;
statetype s_dogdie2;
statetype s_dogdie3;
statetype s_dogdead;
statetype s_ofcstand;
statetype s_ofcpath1;
statetype s_ofcpath1s;
statetype s_ofcpath2;
statetype s_ofcpath3;
statetype s_ofcpath3s;
statetype s_ofcpath4;
statetype s_ofcpain;
statetype s_ofcpain1;
statetype s_ofcshoot1;
statetype s_ofcshoot2;
statetype s_ofcshoot3;
statetype s_ofcchase1;
statetype s_ofcchase1s;
statetype s_ofcchase2;
statetype s_ofcchase3;
statetype s_ofcchase3s;
statetype s_ofcchase4;
statetype s_ofcdie1;
statetype s_ofcdie2;
statetype s_ofcdie3;
statetype s_ofcdie4;
statetype s_ofcdie5;
statetype s_mutstand;
statetype s_mutpath1;
statetype s_mutpath1s;
statetype s_mutpath2;
statetype s_mutpath3;
statetype s_mutpath3s;
statetype s_mutpath4;
statetype s_mutpain;
statetype s_mutpain1;
statetype s_mutshoot1;
statetype s_mutshoot2;
statetype s_mutshoot3;
statetype s_mutshoot4;
statetype s_mutchase1;
statetype s_mutchase1s;
statetype s_mutchase2;
statetype s_mutchase3;
statetype s_mutchase3s;
statetype s_mutchase4;
statetype s_mutdie1;
statetype s_mutdie2;
statetype s_mutdie3;
statetype s_mutdie4;
statetype s_mutdie5;
statetype s_ssstand;
statetype s_sspath1;
statetype s_sspath1s;
statetype s_sspath2;
statetype s_sspath3;
statetype s_sspath3s;
statetype s_sspath4;
statetype s_sspain;
statetype s_sspain1;
statetype s_ssshoot1;
statetype s_ssshoot2;
statetype s_ssshoot3;
statetype s_ssshoot4;
statetype s_ssshoot5;
statetype s_ssshoot6;
statetype s_ssshoot7;
statetype s_ssshoot8;
statetype s_ssshoot9;
statetype s_sschase1;
statetype s_sschase1s;
statetype s_sschase2;
statetype s_sschase3;
statetype s_sschase3s;
statetype s_sschase4;
statetype s_ssdie1;
statetype s_ssdie2;
statetype s_ssdie3;
statetype s_ssdie4;
statetype s_bossstand;
statetype s_bosschase1;
statetype s_bosschase1s;
statetype s_bosschase2;
statetype s_bosschase3;
statetype s_bosschase3s;
statetype s_bosschase4;
statetype s_bossdie1;
statetype s_bossdie2;
statetype s_bossdie3;
statetype s_bossdie4;
statetype s_bossshoot1;
statetype s_bossshoot2;
statetype s_bossshoot3;
statetype s_bossshoot4;
statetype s_bossshoot5;
statetype s_bossshoot6;
statetype s_bossshoot7;
statetype s_bossshoot8;
statetype s_gretelstand;
statetype s_gretelchase1;
statetype s_gretelchase1s;
statetype s_gretelchase2;
statetype s_gretelchase3;
statetype s_gretelchase3s;
statetype s_gretelchase4;
statetype s_greteldie1;
statetype s_greteldie2;
statetype s_greteldie3;
statetype s_greteldie4;
statetype s_gretelshoot1;
statetype s_gretelshoot2;
statetype s_gretelshoot3;
statetype s_gretelshoot4;
statetype s_gretelshoot5;
statetype s_gretelshoot6;
statetype s_gretelshoot7;
statetype s_gretelshoot8;
statetype s_schabbstand;
statetype s_schabbchase1;
statetype s_schabbchase1s;
statetype s_schabbchase2;
statetype s_schabbchase3;
statetype s_schabbchase3s;
statetype s_schabbchase4;
statetype s_schabbdeathcam;
statetype s_schabbdie1;
statetype s_schabbdie2;
statetype s_schabbdie3;
statetype s_schabbdie4;
statetype s_schabbdie5;
statetype s_schabbdie6;
statetype s_schabbshoot1;
statetype s_schabbshoot2;
statetype s_needle1;
statetype s_needle2;
statetype s_needle3;
statetype s_needle4;
statetype s_giftstand;
statetype s_giftchase1;
statetype s_giftchase1s;
statetype s_giftchase2;
statetype s_giftchase3;
statetype s_giftchase3s;
statetype s_giftchase4;
statetype s_giftdeathcam;
statetype s_giftdie1;
statetype s_giftdie2;
statetype s_giftdie3;
statetype s_giftdie4;
statetype s_giftdie5;
statetype s_giftdie6;
statetype s_giftshoot1;
statetype s_giftshoot2;
statetype s_fatstand;
statetype s_fatchase1;
statetype s_fatchase1s;
statetype s_fatchase2;
statetype s_fatchase3;
statetype s_fatchase3s;
statetype s_fatchase4;
statetype s_fatdeathcam;
statetype s_fatdie1;
statetype s_fatdie2;
statetype s_fatdie3;
statetype s_fatdie4;
statetype s_fatdie5;
statetype s_fatdie6;
statetype s_fatshoot1;
statetype s_fatshoot2;
statetype s_fatshoot3;
statetype s_fatshoot4;
statetype s_fatshoot5;
statetype s_fatshoot6;
statetype s_fakestand;
statetype s_fakechase1;
statetype s_fakechase1s;
statetype s_fakechase2;
statetype s_fakechase3;
statetype s_fakechase3s;
statetype s_fakechase4;
statetype s_fakedie1;
statetype s_fakedie2;
statetype s_fakedie3;
statetype s_fakedie4;
statetype s_fakedie5;
statetype s_fakedie6;
statetype s_fakeshoot1;
statetype s_fakeshoot2;
statetype s_fakeshoot3;
statetype s_fakeshoot4;
statetype s_fakeshoot5;
statetype s_fakeshoot6;
statetype s_fakeshoot7;
statetype s_fakeshoot8;
statetype s_fakeshoot9;
statetype s_fire1;
statetype s_fire2;
statetype s_mechastand;
statetype s_mechachase1;
statetype s_mechachase1s;
statetype s_mechachase2;
statetype s_mechachase3;
statetype s_mechachase3s;
statetype s_mechachase4;
statetype s_mechadie1;
statetype s_mechadie2;
statetype s_mechadie3;
statetype s_mechadie4;
statetype s_mechashoot1;
statetype s_mechashoot2;
statetype s_mechashoot3;
statetype s_mechashoot4;
statetype s_mechashoot5;
statetype s_mechashoot6;
statetype s_hitlerchase1;
statetype s_hitlerchase1s;
statetype s_hitlerchase2;
statetype s_hitlerchase3;
statetype s_hitlerchase3s;
statetype s_hitlerchase4;
statetype s_hitlerdeathcam;
statetype s_hitlerdie1;
statetype s_hitlerdie2;
statetype s_hitlerdie3;
statetype s_hitlerdie4;
statetype s_hitlerdie5;
statetype s_hitlerdie6;
statetype s_hitlerdie7;
statetype s_hitlerdie8;
statetype s_hitlerdie9;
statetype s_hitlerdie10;
statetype s_hitlershoot1;
statetype s_hitlershoot2;
statetype s_hitlershoot3;
statetype s_hitlershoot4;
statetype s_hitlershoot5;
statetype s_hitlershoot6;
statetype s_bjrun1;
statetype s_bjrun1s;
statetype s_bjrun2;
statetype s_bjrun3;
statetype s_bjrun3s;
statetype s_bjrun4;
statetype s_bjjump1;
statetype s_bjjump2;
statetype s_bjjump3;
statetype s_bjjump4;
statetype s_deathcam;
#else
extern statetype s_player;
extern statetype s_attack;
extern statetype s_rocket;
extern statetype s_smoke1;
extern statetype s_smoke2;
extern statetype s_smoke3;
extern statetype s_smoke4;
extern statetype s_boom1;
extern statetype s_boom2;
extern statetype s_boom3;
extern statetype s_grdstand;
extern statetype s_grdpath1;
extern statetype s_grdpath1s;
extern statetype s_grdpath2;
extern statetype s_grdpath3;
extern statetype s_grdpath3s;
extern statetype s_grdpath4;
extern statetype s_grdpain;
extern statetype s_grdpain1;
extern statetype s_grdshoot1;
extern statetype s_grdshoot2;
extern statetype s_grdshoot3;
extern statetype s_grdchase1;
extern statetype s_grdchase1s;
extern statetype s_grdchase2;
extern statetype s_grdchase3;
extern statetype s_grdchase3s;
extern statetype s_grdchase4;
extern statetype s_grddie1;
extern statetype s_grddie2;
extern statetype s_grddie3;
extern statetype s_grddie4;
extern statetype s_blinkychase1;
extern statetype s_blinkychase2;
extern statetype s_inkychase1;
extern statetype s_inkychase2;
extern statetype s_pinkychase1;
extern statetype s_pinkychase2;
extern statetype s_clydechase1;
extern statetype s_clydechase2;
extern statetype s_dogpath1;
extern statetype s_dogpath1s;
extern statetype s_dogpath2;
extern statetype s_dogpath3;
extern statetype s_dogpath3s;
extern statetype s_dogpath4;
extern statetype s_dogjump1;
extern statetype s_dogjump2;
extern statetype s_dogjump3;
extern statetype s_dogjump4;
extern statetype s_dogjump5;
extern statetype s_dogchase1;
extern statetype s_dogchase1s;
extern statetype s_dogchase2;
extern statetype s_dogchase3;
extern statetype s_dogchase3s;
extern statetype s_dogchase4;
extern statetype s_dogdie1;
extern statetype s_dogdie2;
extern statetype s_dogdie3;
extern statetype s_dogdead;
extern statetype s_ofcstand;
extern statetype s_ofcpath1;
extern statetype s_ofcpath1s;
extern statetype s_ofcpath2;
extern statetype s_ofcpath3;
extern statetype s_ofcpath3s;
extern statetype s_ofcpath4;
extern statetype s_ofcpain;
extern statetype s_ofcpain1;
extern statetype s_ofcshoot1;
extern statetype s_ofcshoot2;
extern statetype s_ofcshoot3;
extern statetype s_ofcchase1;
extern statetype s_ofcchase1s;
extern statetype s_ofcchase2;
extern statetype s_ofcchase3;
extern statetype s_ofcchase3s;
extern statetype s_ofcchase4;
extern statetype s_ofcdie1;
extern statetype s_ofcdie2;
extern statetype s_ofcdie3;
extern statetype s_ofcdie4;
extern statetype s_ofcdie5;
extern statetype s_mutstand;
extern statetype s_mutpath1;
extern statetype s_mutpath1s;
extern statetype s_mutpath2;
extern statetype s_mutpath3;
extern statetype s_mutpath3s;
extern statetype s_mutpath4;
extern statetype s_mutpain;
extern statetype s_mutpain1;
extern statetype s_mutshoot1;
extern statetype s_mutshoot2;
extern statetype s_mutshoot3;
extern statetype s_mutshoot4;
extern statetype s_mutchase1;
extern statetype s_mutchase1s;
extern statetype s_mutchase2;
extern statetype s_mutchase3;
extern statetype s_mutchase3s;
extern statetype s_mutchase4;
extern statetype s_mutdie1;
extern statetype s_mutdie2;
extern statetype s_mutdie3;
extern statetype s_mutdie4;
extern statetype s_mutdie5;
extern statetype s_ssstand;
extern statetype s_sspath1;
extern statetype s_sspath1s;
extern statetype s_sspath2;
extern statetype s_sspath3;
extern statetype s_sspath3s;
extern statetype s_sspath4;
extern statetype s_sspain;
extern statetype s_sspain1;
extern statetype s_ssshoot1;
extern statetype s_ssshoot2;
extern statetype s_ssshoot3;
extern statetype s_ssshoot4;
extern statetype s_ssshoot5;
extern statetype s_ssshoot6;
extern statetype s_ssshoot7;
extern statetype s_ssshoot8;
extern statetype s_ssshoot9;
extern statetype s_sschase1;
extern statetype s_sschase1s;
extern statetype s_sschase2;
extern statetype s_sschase3;
extern statetype s_sschase3s;
extern statetype s_sschase4;
extern statetype s_ssdie1;
extern statetype s_ssdie2;
extern statetype s_ssdie3;
extern statetype s_ssdie4;
extern statetype s_bossstand;
extern statetype s_bosschase1;
extern statetype s_bosschase1s;
extern statetype s_bosschase2;
extern statetype s_bosschase3;
extern statetype s_bosschase3s;
extern statetype s_bosschase4;
extern statetype s_bossdie1;
extern statetype s_bossdie2;
extern statetype s_bossdie3;
extern statetype s_bossdie4;
extern statetype s_bossshoot1;
extern statetype s_bossshoot2;
extern statetype s_bossshoot3;
extern statetype s_bossshoot4;
extern statetype s_bossshoot5;
extern statetype s_bossshoot6;
extern statetype s_bossshoot7;
extern statetype s_bossshoot8;
extern statetype s_gretelstand;
extern statetype s_gretelchase1;
extern statetype s_gretelchase1s;
extern statetype s_gretelchase2;
extern statetype s_gretelchase3;
extern statetype s_gretelchase3s;
extern statetype s_gretelchase4;
extern statetype s_greteldie1;
extern statetype s_greteldie2;
extern statetype s_greteldie3;
extern statetype s_greteldie4;
extern statetype s_gretelshoot1;
extern statetype s_gretelshoot2;
extern statetype s_gretelshoot3;
extern statetype s_gretelshoot4;
extern statetype s_gretelshoot5;
extern statetype s_gretelshoot6;
extern statetype s_gretelshoot7;
extern statetype s_gretelshoot8;
extern statetype s_schabbstand;
extern statetype s_schabbchase1;
extern statetype s_schabbchase1s;
extern statetype s_schabbchase2;
extern statetype s_schabbchase3;
extern statetype s_schabbchase3s;
extern statetype s_schabbchase4;
extern statetype s_schabbdeathcam;
extern statetype s_schabbdie1;
extern statetype s_schabbdie2;
extern statetype s_schabbdie3;
extern statetype s_schabbdie4;
extern statetype s_schabbdie5;
extern statetype s_schabbdie6;
extern statetype s_schabbshoot1;
extern statetype s_schabbshoot2;
extern statetype s_needle1;
extern statetype s_needle2;
extern statetype s_needle3;
extern statetype s_needle4;
extern statetype s_giftstand;
extern statetype s_giftchase1;
extern statetype s_giftchase1s;
extern statetype s_giftchase2;
extern statetype s_giftchase3;
extern statetype s_giftchase3s;
extern statetype s_giftchase4;
extern statetype s_giftdeathcam;
extern statetype s_giftdie1;
extern statetype s_giftdie2;
extern statetype s_giftdie3;
extern statetype s_giftdie4;
extern statetype s_giftdie5;
extern statetype s_giftdie6;
extern statetype s_giftshoot1;
extern statetype s_giftshoot2;
extern statetype s_fatstand;
extern statetype s_fatchase1;
extern statetype s_fatchase1s;
extern statetype s_fatchase2;
extern statetype s_fatchase3;
extern statetype s_fatchase3s;
extern statetype s_fatchase4;
extern statetype s_fatdeathcam;
extern statetype s_fatdie1;
extern statetype s_fatdie2;
extern statetype s_fatdie3;
extern statetype s_fatdie4;
extern statetype s_fatdie5;
extern statetype s_fatdie6;
extern statetype s_fatshoot1;
extern statetype s_fatshoot2;
extern statetype s_fatshoot3;
extern statetype s_fatshoot4;
extern statetype s_fatshoot5;
extern statetype s_fatshoot6;
extern statetype s_fakestand;
extern statetype s_fakechase1;
extern statetype s_fakechase1s;
extern statetype s_fakechase2;
extern statetype s_fakechase3;
extern statetype s_fakechase3s;
extern statetype s_fakechase4;
extern statetype s_fakedie1;
extern statetype s_fakedie2;
extern statetype s_fakedie3;
extern statetype s_fakedie4;
extern statetype s_fakedie5;
extern statetype s_fakedie6;
extern statetype s_fakeshoot1;
extern statetype s_fakeshoot2;
extern statetype s_fakeshoot3;
extern statetype s_fakeshoot4;
extern statetype s_fakeshoot5;
extern statetype s_fakeshoot6;
extern statetype s_fakeshoot7;
extern statetype s_fakeshoot8;
extern statetype s_fakeshoot9;
extern statetype s_fire1;
extern statetype s_fire2;
extern statetype s_mechastand;
extern statetype s_mechachase1;
extern statetype s_mechachase1s;
extern statetype s_mechachase2;
extern statetype s_mechachase3;
extern statetype s_mechachase3s;
extern statetype s_mechachase4;
extern statetype s_mechadie1;
extern statetype s_mechadie2;
extern statetype s_mechadie3;
extern statetype s_mechadie4;
extern statetype s_mechashoot1;
extern statetype s_mechashoot2;
extern statetype s_mechashoot3;
extern statetype s_mechashoot4;
extern statetype s_mechashoot5;
extern statetype s_mechashoot6;
extern statetype s_hitlerchase1;
extern statetype s_hitlerchase1s;
extern statetype s_hitlerchase2;
extern statetype s_hitlerchase3;
extern statetype s_hitlerchase3s;
extern statetype s_hitlerchase4;
extern statetype s_hitlerdeathcam;
extern statetype s_hitlerdie1;
extern statetype s_hitlerdie2;
extern statetype s_hitlerdie3;
extern statetype s_hitlerdie4;
extern statetype s_hitlerdie5;
extern statetype s_hitlerdie6;
extern statetype s_hitlerdie7;
extern statetype s_hitlerdie8;
extern statetype s_hitlerdie9;
extern statetype s_hitlerdie10;
extern statetype s_hitlershoot1;
extern statetype s_hitlershoot2;
extern statetype s_hitlershoot3;
extern statetype s_hitlershoot4;
extern statetype s_hitlershoot5;
extern statetype s_hitlershoot6;
extern statetype s_bjrun1;
extern statetype s_bjrun1s;
extern statetype s_bjrun2;
extern statetype s_bjrun3;
extern statetype s_bjrun3s;
extern statetype s_bjrun4;
extern statetype s_bjjump1;
extern statetype s_bjjump2;
extern statetype s_bjjump3;
extern statetype s_bjjump4;
extern statetype s_deathcam;
#endif

static const statetype * const wl_state_table[305] = {
    &s_player,
    &s_attack,
    &s_rocket,
    &s_smoke1,
    &s_smoke2,
    &s_smoke3,
    &s_smoke4,
    &s_boom1,
    &s_boom2,
    &s_boom3,
    &s_grdstand,
    &s_grdpath1,
    &s_grdpath1s,
    &s_grdpath2,
    &s_grdpath3,
    &s_grdpath3s,
    &s_grdpath4,
    &s_grdpain,
    &s_grdpain1,
    &s_grdshoot1,
    &s_grdshoot2,
    &s_grdshoot3,
    &s_grdchase1,
    &s_grdchase1s,
    &s_grdchase2,
    &s_grdchase3,
    &s_grdchase3s,
    &s_grdchase4,
    &s_grddie1,
    &s_grddie2,
    &s_grddie3,
    &s_grddie4,
    &s_blinkychase1,
    &s_blinkychase2,
    &s_inkychase1,
    &s_inkychase2,
    &s_pinkychase1,
    &s_pinkychase2,
    &s_clydechase1,
    &s_clydechase2,
    &s_dogpath1,
    &s_dogpath1s,
    &s_dogpath2,
    &s_dogpath3,
    &s_dogpath3s,
    &s_dogpath4,
    &s_dogjump1,
    &s_dogjump2,
    &s_dogjump3,
    &s_dogjump4,
    &s_dogjump5,
    &s_dogchase1,
    &s_dogchase1s,
    &s_dogchase2,
    &s_dogchase3,
    &s_dogchase3s,
    &s_dogchase4,
    &s_dogdie1,
    &s_dogdie2,
    &s_dogdie3,
    &s_dogdead,
    &s_ofcstand,
    &s_ofcpath1,
    &s_ofcpath1s,
    &s_ofcpath2,
    &s_ofcpath3,
    &s_ofcpath3s,
    &s_ofcpath4,
    &s_ofcpain,
    &s_ofcpain1,
    &s_ofcshoot1,
    &s_ofcshoot2,
    &s_ofcshoot3,
    &s_ofcchase1,
    &s_ofcchase1s,
    &s_ofcchase2,
    &s_ofcchase3,
    &s_ofcchase3s,
    &s_ofcchase4,
    &s_ofcdie1,
    &s_ofcdie2,
    &s_ofcdie3,
    &s_ofcdie4,
    &s_ofcdie5,
    &s_mutstand,
    &s_mutpath1,
    &s_mutpath1s,
    &s_mutpath2,
    &s_mutpath3,
    &s_mutpath3s,
    &s_mutpath4,
    &s_mutpain,
    &s_mutpain1,
    &s_mutshoot1,
    &s_mutshoot2,
    &s_mutshoot3,
    &s_mutshoot4,
    &s_mutchase1,
    &s_mutchase1s,
    &s_mutchase2,
    &s_mutchase3,
    &s_mutchase3s,
    &s_mutchase4,
    &s_mutdie1,
    &s_mutdie2,
    &s_mutdie3,
    &s_mutdie4,
    &s_mutdie5,
    &s_ssstand,
    &s_sspath1,
    &s_sspath1s,
    &s_sspath2,
    &s_sspath3,
    &s_sspath3s,
    &s_sspath4,
    &s_sspain,
    &s_sspain1,
    &s_ssshoot1,
    &s_ssshoot2,
    &s_ssshoot3,
    &s_ssshoot4,
    &s_ssshoot5,
    &s_ssshoot6,
    &s_ssshoot7,
    &s_ssshoot8,
    &s_ssshoot9,
    &s_sschase1,
    &s_sschase1s,
    &s_sschase2,
    &s_sschase3,
    &s_sschase3s,
    &s_sschase4,
    &s_ssdie1,
    &s_ssdie2,
    &s_ssdie3,
    &s_ssdie4,
    &s_bossstand,
    &s_bosschase1,
    &s_bosschase1s,
    &s_bosschase2,
    &s_bosschase3,
    &s_bosschase3s,
    &s_bosschase4,
    &s_bossdie1,
    &s_bossdie2,
    &s_bossdie3,
    &s_bossdie4,
    &s_bossshoot1,
    &s_bossshoot2,
    &s_bossshoot3,
    &s_bossshoot4,
    &s_bossshoot5,
    &s_bossshoot6,
    &s_bossshoot7,
    &s_bossshoot8,
    &s_gretelstand,
    &s_gretelchase1,
    &s_gretelchase1s,
    &s_gretelchase2,
    &s_gretelchase3,
    &s_gretelchase3s,
    &s_gretelchase4,
    &s_greteldie1,
    &s_greteldie2,
    &s_greteldie3,
    &s_greteldie4,
    &s_gretelshoot1,
    &s_gretelshoot2,
    &s_gretelshoot3,
    &s_gretelshoot4,
    &s_gretelshoot5,
    &s_gretelshoot6,
    &s_gretelshoot7,
    &s_gretelshoot8,
    &s_schabbstand,
    &s_schabbchase1,
    &s_schabbchase1s,
    &s_schabbchase2,
    &s_schabbchase3,
    &s_schabbchase3s,
    &s_schabbchase4,
    &s_schabbdeathcam,
    &s_schabbdie1,
    &s_schabbdie2,
    &s_schabbdie3,
    &s_schabbdie4,
    &s_schabbdie5,
    &s_schabbdie6,
    &s_schabbshoot1,
    &s_schabbshoot2,
    &s_needle1,
    &s_needle2,
    &s_needle3,
    &s_needle4,
    &s_giftstand,
    &s_giftchase1,
    &s_giftchase1s,
    &s_giftchase2,
    &s_giftchase3,
    &s_giftchase3s,
    &s_giftchase4,
    &s_giftdeathcam,
    &s_giftdie1,
    &s_giftdie2,
    &s_giftdie3,
    &s_giftdie4,
    &s_giftdie5,
    &s_giftdie6,
    &s_giftshoot1,
    &s_giftshoot2,
    &s_fatstand,
    &s_fatchase1,
    &s_fatchase1s,
    &s_fatchase2,
    &s_fatchase3,
    &s_fatchase3s,
    &s_fatchase4,
    &s_fatdeathcam,
    &s_fatdie1,
    &s_fatdie2,
    &s_fatdie3,
    &s_fatdie4,
    &s_fatdie5,
    &s_fatdie6,
    &s_fatshoot1,
    &s_fatshoot2,
    &s_fatshoot3,
    &s_fatshoot4,
    &s_fatshoot5,
    &s_fatshoot6,
    &s_fakestand,
    &s_fakechase1,
    &s_fakechase1s,
    &s_fakechase2,
    &s_fakechase3,
    &s_fakechase3s,
    &s_fakechase4,
    &s_fakedie1,
    &s_fakedie2,
    &s_fakedie3,
    &s_fakedie4,
    &s_fakedie5,
    &s_fakedie6,
    &s_fakeshoot1,
    &s_fakeshoot2,
    &s_fakeshoot3,
    &s_fakeshoot4,
    &s_fakeshoot5,
    &s_fakeshoot6,
    &s_fakeshoot7,
    &s_fakeshoot8,
    &s_fakeshoot9,
    &s_fire1,
    &s_fire2,
    &s_mechastand,
    &s_mechachase1,
    &s_mechachase1s,
    &s_mechachase2,
    &s_mechachase3,
    &s_mechachase3s,
    &s_mechachase4,
    &s_mechadie1,
    &s_mechadie2,
    &s_mechadie3,
    &s_mechadie4,
    &s_mechashoot1,
    &s_mechashoot2,
    &s_mechashoot3,
    &s_mechashoot4,
    &s_mechashoot5,
    &s_mechashoot6,
    &s_hitlerchase1,
    &s_hitlerchase1s,
    &s_hitlerchase2,
    &s_hitlerchase3,
    &s_hitlerchase3s,
    &s_hitlerchase4,
    &s_hitlerdeathcam,
    &s_hitlerdie1,
    &s_hitlerdie2,
    &s_hitlerdie3,
    &s_hitlerdie4,
    &s_hitlerdie5,
    &s_hitlerdie6,
    &s_hitlerdie7,
    &s_hitlerdie8,
    &s_hitlerdie9,
    &s_hitlerdie10,
    &s_hitlershoot1,
    &s_hitlershoot2,
    &s_hitlershoot3,
    &s_hitlershoot4,
    &s_hitlershoot5,
    &s_hitlershoot6,
    &s_bjrun1,
    &s_bjrun1s,
    &s_bjrun2,
    &s_bjrun3,
    &s_bjrun3s,
    &s_bjrun4,
    &s_bjjump1,
    &s_bjjump2,
    &s_bjjump3,
    &s_bjjump4,
    &s_deathcam,
};

#endif /* !SPEAR */
#endif /* WL_STATES_H */
