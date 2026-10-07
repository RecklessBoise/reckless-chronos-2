#!/usr/bin/env python3
"""
Factory preset generator for Reckless Chronos 2.

Every program starts from a hand-written *recipe* (engine + macros + filter/envelopes/FX).
Each recipe is then rendered in a few musically useful *flavours* (room/hall, chorus,
drive, mono/legato, ...) to build a large but coherent bank, sorted in the 16 categories.
Combis layer / split those programs on up to four timbres.

    python3 tools/preset_gen.py                 # writes Presets/factory.json (levels balanced from tools/measured_levels.csv)
    python3 tools/preset_gen.py --no-levels     # raw recipe levels; render with `RC2Tests --loudness tools/measured_levels.csv`

The C++ loader rejects unknown parameter names, and RC2Tests renders every preset.
"""
import argparse
import copy
import csv
import json
import math
import os
import random

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "Presets", "factory.json")

TIMBRE_IDS = """engine e1 e2 e3 e4 e5 e6 e7 e8 transpose fine voiceMode glide unison detune pbRange pitchEg
fltType cutoff reso fltEnv fltKey fltVel drive fA fD fS fR aA aD aS aR ampVel lfoWave lfoRate lfoDelay
lfoPitch lfoFilter lfoAmp wheelVib wheelCut atVib ifx1Type ifx1A ifx1B ifx1Mix ifx2Type ifx2A ifx2B ifx2Mix
level pan send1 send2 on keyLo keyHi velLo velHi vector""".split()
GLOBAL_IDS = """dlySync dlyTime dlyFb dlyTone dlyPing dlyRet revSize revDamp revPre revRet
arpOn arpMode arpRate arpOct arpGate arpSwing arpLatch""".split()

PROGRAM_CATS = ["Keyboard", "Organ", "Bell/Mallet", "Strings", "Vocal/Airy", "Brass", "Woodwind/Reed",
                "Guitar/Plucked", "Bass", "Slow Synth", "Fast Synth", "Lead Synth", "Motion Synth", "SE",
                "Hit/Arpg", "Drums"]
COMBI_CATS = ["Keyboard", "Organ", "Bell/Mallet", "Strings", "Pads", "Brass/Reed", "Orchestral", "World",
              "Guitar", "Bass Splits", "Synth", "Lead", "Motion", "SE/Hits", "Arpeggio", "Drums/Splits"]

# ---------------------------------------------------------------------------------------------
# helpers for engine-specific macro values
WAVES = ["Saw", "Square", "Triangle", "Sine", "Organ", "Strings", "Brass", "Aah", "Ooh", "Eee", "Clarinet",
         "Oboe", "Flute", "Glass", "Digital1", "Digital2", "Pulse25", "Pulse10", "Bass", "Reed", "Harp",
         "Cello", "BrightVox", "Sync"]
FM_RATIOS = [0.5, 1.0, 1.41, 2.0, 2.76, 3.0, 3.5, 4.0, 5.0, 5.4, 6.0, 7.0, 7.13, 8.0, 9.0, 11.0, 14.0]
INTERVALS = [-24, -12, -7, -5, 0, 3, 4, 5, 7, 12, 19, 24]


def idx(i, n):
    return round((i + 0.5) / n, 4)


def wave(name):
    return idx(WAVES.index(name), len(WAVES))


def ratio(r):
    return idx(FM_RATIOS.index(r), len(FM_RATIOS))


def interval(semis):
    return idx(INTERVALS.index(semis), len(INTERVALS))


def algo(n):  # 1..8
    return idx(n - 1, 8)


SAW, SQR, TRI, SINE = 0.0, 0.333, 0.667, 1.0
BOCT = {0: idx(0, 4), 12: idx(1, 4), 19: idx(2, 4), 24: idx(3, 4)}


def db(level):  # drawbar 0..8 -> macro
    return round(level / 8.0, 4)


def drawbars(reg):
    """'888000000' (16' 5 1/3' 8' 4' 2 2/3' 2' 1 3/5' 1 1/3' 1') -> e1..e7"""
    v = [int(c) for c in reg]
    upper = max(v[6:9])
    d = {"e%d" % (i + 1): db(v[i]) for i in range(6)}
    d["e7"] = db(upper)
    return d


# ---------------------------------------------------------------------------------------------
# base templates per engine
def base(engine, **kw):
    p = {"engine": engine, "fltType": "LP24", "cutoff": 8000, "reso": 0.1, "fltEnv": 0.0, "fltKey": 0.5,
         "fltVel": 0.3, "aA": 0.003, "aD": 0.5, "aS": 1.0, "aR": 0.3, "fA": 0.005, "fD": 0.4, "fS": 0.3,
         "fR": 0.3, "send1": 0.0, "send2": 0.18, "lfoPitch": 0.0, "wheelVib": 0.3}
    p.update(kw)
    return p


def piano(**kw):
    p = base("Grand Piano", fltType="Off", e1=0.62, e2=0.7, e3=0.35, e4=0.5, e5=0.3, e6=0.35, e7=0.7, e8=0.55,
             aA=0.001, aD=20, aS=1.0, aR=0.45, ampVel=0.85, send2=0.22, wheelVib=0.0)
    p.update(kw)
    return p


def epiano(**kw):
    p = base("Electric Piano", fltType="Off", e1=0.0, e2=0.5, e3=0.5, e4=0.6, e5=0.35, e6=0.3, e7=0.0, e8=0.45,
             aA=0.001, aD=20, aS=1.0, aR=0.35, ampVel=0.8, send2=0.15, wheelVib=0.0)
    p.update(kw)
    return p


def organ(reg="888000000", perc=0.0, **kw):
    p = base("Tonewheel Organ", fltType="Off", aA=0.004, aD=1, aS=1.0, aR=0.06, ampVel=0.0, send2=0.12,
             wheelVib=0.0, e8=perc, ifx1Type="Rotary", ifx1A=0.2, ifx1B=0.25, ifx1Mix=1.0)
    p.update(drawbars(reg))
    p.update(kw)
    return p


def wavep(a="Saw", b="Saw", morph=0.0, boct=0, **kw):
    p = base("Wave ROM", e1=wave(a), e2=wave(b), e3=morph, e4=0.0, e5=0.0, e6=0.0, e7=0.0, e8=BOCT[boct])
    p.update(kw)
    return p


def analog(o1=SAW, o2=SAW, mix=0.5, iv=0, fine=0.15, sub=0.0, noise=0.0, pw=0.0, **kw):
    p = base("Analog", e1=o1, e2=o2, e3=fine, e4=interval(iv), e5=mix, e6=sub, e7=noise, e8=pw)
    p.update(kw)
    return p


def twin(w1=1, w2=0, pitch=0.0, mix=0.5, hpf=0.0, peak=0.0, scream=0.2, pw=0.0, **kw):
    p = base("Twin Filter", fltType="MS-LP", e1=idx(w1, 4), e2=idx(w2, 4), e3=round(0.5 + pitch / 24.0, 4),
             e4=mix, e5=hpf, e6=peak, e7=scream, e8=pw)
    p.update(kw)
    return p


def poly6(wav=0, pw=0.3, pwmRate=0.4, pwmDepth=0.0, sub=0.0, noise=0.0, drift=0.3, ens=0.0, **kw):
    p = base("Vintage Poly", e1=idx(wav, 3), e2=pw, e3=pwmRate, e4=pwmDepth, e5=sub, e6=noise, e7=drift, e8=ens)
    p.update(kw)
    return p


def fm(a=1, r2=1.0, i2=0.3, r3=1.0, i3=0.0, fb=0.0, dec=0.4, vel=0.5, **kw):
    p = base("FM Matrix", fltType="Off", e1=algo(a), e2=ratio(r2), e3=i2, e4=ratio(r3), e5=i3, e6=fb, e7=dec, e8=vel)
    p.update(kw)
    return p


def pluck(pos=0.3, excite=0.6, decay=0.55, bright=0.6, stiff=0.1, pick=0.3, body=0.4, bow=0.0, **kw):
    p = base("Plucked String", fltType="Off", e1=pos, e2=excite, e3=decay, e4=bright, e5=stiff, e6=pick, e7=body,
             e8=bow, aA=0.001, aD=20, aS=1.0, aR=0.25, ampVel=0.6, wheelVib=0.15)
    p.update(kw)
    return p


def drums(**kw):
    p = base("Drum Kit", fltType="Off", e1=0.4, e2=0.45, e3=0.5, e4=0.5, e5=0.4, e6=0.5, e7=0.5, e8=0.5,
             aA=0.001, aD=20, aS=1.0, aR=0.1, ampVel=0.7, send2=0.1, wheelVib=0.0)
    p.update(kw)
    return p


def fx(p, typ, a=0.5, b=0.5, mix=0.5):
    """put an insert effect in the first free slot"""
    for slot in ("ifx1", "ifx2"):
        if p.get(slot + "Type", "Off") in ("Off", typ):
            p[slot + "Type"], p[slot + "A"], p[slot + "B"], p[slot + "Mix"] = typ, a, b, mix
            return p
    return p  # both slots busy: keep the recipe as designed


def has_fx(p, typ):
    return p.get("ifx1Type") == typ or p.get("ifx2Type") == typ


# ---------------------------------------------------------------------------------------------
# flavours: (suffix, function(program dict, globals dict))
def fl_hall(p, g):
    p["send2"] = max(p.get("send2", 0.2), 0.42); g.update(revSize=0.88, revDamp=0.35, revPre=35)


def fl_room(p, g):
    p["send2"] = 0.16; g.update(revSize=0.35, revDamp=0.55, revPre=8)


def fl_dry(p, g):
    p["send2"] = 0.03; p["send1"] = 0.0


def fl_chorus(p, g):
    fx(p, "Chorus", 0.3, 0.55, 0.45)


def fl_ensemble(p, g):
    fx(p, "Ensemble", 0.4, 0.6, 0.55)


def fl_phaser(p, g):
    fx(p, "Phaser", 0.25, 0.45, 0.5)


def fl_drive(p, g):
    fx(p, "Overdrive", 0.45, 0.6, 0.8)


def fl_amp(p, g):
    fx(p, "Amp Sim", 0.55, 0.55, 1.0)


def fl_echo(p, g):
    p["send1"] = max(p.get("send1", 0.0), 0.28); g.update(dlySync="1/8D", dlyFb=0.38, dlyTone=0.55, dlyPing=0.7)


def fl_space(p, g):
    fl_hall(p, g); p["send1"] = 0.32; g.update(dlySync="1/4", dlyFb=0.5, dlyTone=0.45, dlyPing=0.9)


def fl_bright(p, g):
    if p.get("fltType", "Off") != "Off":
        p["cutoff"] = min(18000, p.get("cutoff", 8000) * 1.8)
    else:
        fx(p, "Tone EQ", 0.5, 0.72, 1.0)


def fl_dark(p, g):
    if p.get("fltType", "Off") != "Off":
        p["cutoff"] = max(120, p.get("cutoff", 8000) * 0.45)
    else:
        fx(p, "Tone EQ", 0.55, 0.25, 1.0)


def fl_soft(p, g):
    p["aA"] = max(p.get("aA", 0.003), 0.08); p["ampVel"] = 0.9


def fl_mono(p, g):
    p["voiceMode"] = "Legato"; p["glide"] = 0.06


def fl_glide(p, g):
    p["voiceMode"] = "Legato"; p["glide"] = 0.18


def fl_stack(p, g):
    if p["engine"] in ("Analog", "Vintage Poly", "Wave ROM"):
        p["unison"] = 3; p["detune"] = 0.32
    else:
        fx(p, "Ensemble", 0.4, 0.7, 0.6)


def fl_wide(p, g):
    fx(p, "Chorus", 0.2, 0.8, 0.5); p["send2"] = max(p.get("send2", 0.2), 0.3)


def fl_slow(p, g):
    p["aA"] = max(p.get("aA", 0.003), 0.6); p["aR"] = max(p.get("aR", 0.3), 1.6)


def fl_motion(p, g):
    p.update(lfoWave="Triangle", lfoRate=0.25, lfoFilter=0.25, lfoDelay=0.0)


def fl_fast(p, g):
    p["lfoRate"] = min(20, p.get("lfoRate", 5) * 2.2)


def fl_tremolo(p, g):
    fx(p, "Tremolo", 0.45, 0.6, 1.0)


def fl_wah(p, g):
    fx(p, "Auto Wah", 0.6, 0.6, 0.8)


def fl_lofi(p, g):
    fx(p, "Lo-Fi", 0.45, 0.3, 0.6)


def fl_comp(p, g):
    fx(p, "Compressor", 0.6, 0.5, 1.0)


def fl_rotfast(p, g):
    p["ifx1Type"], p["ifx1A"] = "Rotary", 0.8


def fl_rotdrive(p, g):
    p["ifx1Type"], p["ifx1B"] = "Rotary", 0.75


def fl_perc(p, g):
    p["e8"] = 0.75


def fl_tight(p, g):
    p["e2"] = max(0.1, p.get("e2", 0.45) - 0.25); p["e5"] = max(0.1, p.get("e5", 0.4) - 0.2)


def fl_deep(p, g):
    p["e1"] = max(0.05, p.get("e1", 0.4) - 0.25); p["e2"] = min(1.0, p.get("e2", 0.45) + 0.3)


def fl_short(p, g):
    p["aR"] = min(p.get("aR", 0.3), 0.12)
    if p["engine"] in ("Plucked String",):
        p["e3"] = max(0.05, p.get("e3", 0.5) - 0.3)
    else:
        p["aS"] = 0.0; p["aD"] = 0.35


def fl_long(p, g):
    p["aR"] = max(p.get("aR", 0.3), 1.2)
    if p["engine"] in ("Plucked String", "Electric Piano", "Grand Piano"):
        p["e3" if p["engine"] == "Plucked String" else ("e4" if p["engine"] == "Electric Piano" else "e2")] = 0.95


def fl_octave(p, g):
    p["transpose"] = 12


def fl_oct_down(p, g):
    p["transpose"] = -12


def fl_arp_up(p, g):
    g.update(arpOn=True, arpMode="Up", arpRate="1/16", arpOct=2, arpGate=0.45)


def fl_arp_ud(p, g):
    g.update(arpOn=True, arpMode="Up-Down", arpRate="1/8", arpOct=3, arpGate=0.6)


def fl_arp_rand(p, g):
    g.update(arpOn=True, arpMode="Random", arpRate="1/16", arpOct=2, arpGate=0.35)


def fl_arp_trip(p, g):
    g.update(arpOn=True, arpMode="Up", arpRate="1/8T", arpOct=2, arpGate=0.5, arpSwing=0.0)


FLAVOURS = {
    "Hall": fl_hall, "Room": fl_room, "Dry": fl_dry, "Chorus": fl_chorus, "Ensemble": fl_ensemble,
    "Phaser": fl_phaser, "Drive": fl_drive, "Amp": fl_amp, "Echo": fl_echo, "Space": fl_space,
    "Bright": fl_bright, "Dark": fl_dark, "Soft": fl_soft, "Mono": fl_mono, "Glide": fl_glide,
    "Stack": fl_stack, "Wide": fl_wide, "Slow": fl_slow, "Motion": fl_motion, "Fast": fl_fast,
    "Tremolo": fl_tremolo, "Wah": fl_wah, "Lo-Fi": fl_lofi, "Comp": fl_comp, "Fast Rotor": fl_rotfast,
    "Dirty": fl_rotdrive, "Perc": fl_perc, "Tight": fl_tight, "Deep": fl_deep, "Short": fl_short,
    "Long": fl_long, "8va": fl_octave, "8vb": fl_oct_down, "Arp": fl_arp_up, "UpDown": fl_arp_ud,
    "Random": fl_arp_rand, "Triplet": fl_arp_trip,
}

# ---------------------------------------------------------------------------------------------
# RECIPES  (name, params, optional globals)
R = {c: [] for c in PROGRAM_CATS}


ALIASES = {
    "Analog": {"pw": "e8", "sub": "e6", "noise": "e7", "fine": "e3", "mix": "e5"},
    "Vintage Poly": {"pw": "e2", "sub": "e5", "noise": "e6", "drift": "e7", "ens": "e8"},
    "Twin Filter": {"pw": "e8", "scream": "e7", "mix": "e4"},
}


def normalize(p):
    """translate friendly macro names passed through wrapper **kwargs"""
    al = ALIASES.get(p["engine"], {})
    for k in list(p.keys()):
        if k in al:
            p[al[k]] = p.pop(k)
        elif k == "iv":
            p["e4"] = interval(p.pop(k))
    return p


def add(cat, name, p, g=None):
    R[cat].append((name, normalize(p), g or {}))


# ---- Keyboard -------------------------------------------------------------------------------
K = "Keyboard"
add(K, "Concert Grand", piano())
add(K, "Bright Grand", fx(piano(e1=0.86, e7=0.8, e5=0.35), "Tone EQ", 0.5, 0.68, 1.0))
add(K, "Warm Grand", fx(piano(e1=0.42, e2=0.75, e6=0.45), "Tone EQ", 0.62, 0.35, 1.0))
add(K, "Jazz Grand", piano(e1=0.55, e2=0.6, e6=0.5, e8=0.45, send2=0.15), {"revSize": 0.35})
add(K, "Rock Piano", fx(piano(e1=0.92, e5=0.5, e7=0.85, aR=0.3), "Compressor", 0.6, 0.6, 1.0))
add(K, "Ballad Grand", piano(e1=0.48, e2=0.92, aR=0.8, send2=0.4, send1=0.08), {"revSize": 0.85, "dlySync": "1/4", "dlyFb": 0.3})
add(K, "Honky Tonk", fx(piano(e3=0.95, e1=0.8, e2=0.5, e4=0.7), "Tone EQ", 0.45, 0.62, 1.0))
add(K, "Upright Piano", fx(piano(e2=0.45, e6=0.75, e8=0.3, e4=0.65), "Tone EQ", 0.62, 0.45, 1.0), {"revSize": 0.35})
add(K, "Felt Piano", fx(piano(e1=0.12, e5=0.12, e7=0.4, e6=0.55, aR=0.6, send2=0.35), "Tone EQ", 0.55, 0.22, 1.0))
add(K, "Dark Grand", piano(e1=0.3, e7=0.55))
add(K, "Stage Grand", fx(piano(e1=0.75, e5=0.4), "Chorus", 0.2, 0.3, 0.25))
add(K, "Lo-Fi Keys", fx(fx(piano(e1=0.5, e3=0.5), "Lo-Fi", 0.55, 0.25, 0.7), "Tone EQ", 0.55, 0.3, 1.0))
add(K, "Ambient Grand", piano(e1=0.4, e2=0.95, aR=2.5, send2=0.6, send1=0.25), {"revSize": 0.95, "dlySync": "1/4D", "dlyFb": 0.45})
add(K, "Classic Tine", epiano())
add(K, "Suitcase Tine", epiano(e7=0.6, e8=0.42, e5=0.4))
add(K, "Bright Tine", fx(epiano(e2=0.8, e3=0.8, e5=0.45), "Chorus", 0.3, 0.4, 0.3))
add(K, "Mellow Tine", epiano(e2=0.25, e3=0.2, e5=0.15, e4=0.7))
add(K, "Phaser Tine", fx(epiano(e2=0.6), "Phaser", 0.22, 0.5, 0.6))
add(K, "Dyno Tine", fx(fx(epiano(e3=1.0, e2=0.9, e5=0.5), "Chorus", 0.35, 0.45, 0.45), "Tone EQ", 0.45, 0.7, 1.0))
add(K, "Reed Piano", epiano(e1=1.0, e5=0.45, e2=0.55, e3=0.3))
add(K, "Reed Drive", fx(epiano(e1=1.0, e5=0.85, e2=0.7), "Overdrive", 0.4, 0.55, 0.7))
add(K, "Reed Tremolo", epiano(e1=1.0, e7=0.55, e8=0.55))
add(K, "Pianet Keys", epiano(e1=0.65, e4=0.2, e3=0.2, e2=0.7))
add(K, "FM E.Piano", fm(a=6, r2=1.0, i2=0.25, r3=14.0, i3=0.12, dec=0.25, vel=0.8, aD=3.0, aS=0.0, aR=0.5, ampVel=0.8))
add(K, "Clavi", fx(pluck(pos=0.08, excite=0.95, decay=0.25, bright=0.85, stiff=0.2, pick=0.5, body=0.0, aR=0.06), "Auto Wah", 0.35, 0.5, 0.45))
add(K, "Harpsichord", fx(pluck(pos=0.06, excite=1.0, decay=0.45, bright=0.92, stiff=0.3, pick=0.7, body=0.25, aR=0.4), "Chorus", 0.2, 0.2, 0.2))
add(K, "Digital Piano", fm(a=4, r2=2.0, i2=0.35, r3=1.0, i3=0.15, dec=0.35, vel=0.7, aD=4.0, aS=0.0, aR=0.4, ampVel=0.85))

# ---- Organ ----------------------------------------------------------------------------------
O = "Organ"
add(O, "Full Drawbars", organ("888888888"))
add(O, "Jazz Organ", organ("888000000", perc=0.8))
add(O, "Gospel Organ", fx(organ("888800008"), "Overdrive", 0.3, 0.55, 0.5))
add(O, "Rock Organ", fx(organ("888800000", ifx1B=0.7), "Overdrive", 0.55, 0.5, 0.8))
add(O, "Ballad Organ", organ("838000000", ifx1A=0.1))
add(O, "Soul Organ", organ("886000000", perc=0.7))
add(O, "Flute Stop 8'", organ("008000000", ifx1Type="Chorus", ifx1A=0.4, ifx1B=0.3, ifx1Mix=0.4))
add(O, "Mellow 16+8", organ("808000000"))
add(O, "Whiter Shade", organ("688600000"))
add(O, "Bright Top", organ("008888888"))
add(O, "Blues Organ", fx(organ("888600000", perc=0.5, ifx1B=0.55), "Overdrive", 0.35, 0.45, 0.6))
add(O, "Reggae Bubble", organ("008800000", perc=0.9, aR=0.03))
add(O, "Theatre Organ", fx(organ("878646454", lfoPitch=0.12, lfoRate=6.2, lfoDelay=0.15, ifx1Type="Off"), "Tremolo", 0.55, 0.3, 1.0))
add(O, "Pipe Organ Full", wavep("Organ", "Reed", 0.4, 12, aA=0.06, aR=0.9, fltType="Off", send2=0.55, unison=2, detune=0.08), {"revSize": 0.95, "revPre": 40})
add(O, "Pipe Flutes", wavep("Flute", "Organ", 0.3, 12, aA=0.05, aR=0.8, fltType="Off", send2=0.5, e6=0.25), {"revSize": 0.92, "revPre": 40})
add(O, "Reed Pipes", wavep("Reed", "Oboe", 0.5, 0, aA=0.04, aR=0.7, fltType="LP12", cutoff=6000, send2=0.5), {"revSize": 0.9})
add(O, "Positive Organ", wavep("Flute", "Organ", 0.6, 12, aA=0.03, aR=0.4, fltType="Off", send2=0.3, e6=0.4), {"revSize": 0.6})
add(O, "Combo Organ", analog(SQR, TRI, 0.4, 12, fine=0.05, cutoff=7000, fltType="LP12", aA=0.004, aR=0.05, lfoPitch=0.08, lfoRate=6.5, lfoDelay=0.0))
add(O, "Transistor Organ", fx(analog(SAW, SQR, 0.5, 12, fine=0.0, cutoff=5000, fltType="LP12", aA=0.003, aR=0.04, lfoPitch=0.1, lfoRate=7.0, lfoDelay=0.0), "Chorus", 0.4, 0.3, 0.3))
add(O, "Funky Perc", organ("800000000", perc=1.0, aR=0.03))
add(O, "Ethereal Organ", organ("006060406", ifx1A=0.05, send2=0.4, send1=0.15), {"revSize": 0.9, "dlySync": "1/4"})

# ---- Bell / Mallet --------------------------------------------------------------------------
B = "Bell/Mallet"
def bellenv(dec=3.0, rel=1.2):
    return dict(aA=0.001, aD=dec, aS=0.0, aR=rel, ampVel=0.8)
add(B, "Tubular Bells", fm(a=1, r2=3.5, i2=0.35, r3=1.41, i3=0.1, dec=0.6, send2=0.45, **bellenv(6, 3)), {"revSize": 0.9})
add(B, "Glockenspiel", fm(a=6, r2=7.0, i2=0.18, r3=14.0, i3=0.08, dec=0.2, **bellenv(1.8, 1.0)))
add(B, "Vibraphone", fx(fm(a=6, r2=4.0, i2=0.12, r3=11.0, i3=0.05, dec=0.15, **bellenv(3.5, 0.8)), "Tremolo", 0.55, 0.45, 1.0))
add(B, "Marimba", fm(a=1, r2=4.0, i2=0.25, r3=1.0, i3=0.0, dec=0.08, **bellenv(0.7, 0.4)))
add(B, "Xylophone", fm(a=1, r2=3.0, i2=0.3, r3=7.0, i3=0.1, dec=0.05, **bellenv(0.35, 0.25)))
add(B, "Kalimba", pluck(pos=0.12, excite=0.55, decay=0.3, bright=0.45, stiff=0.6, pick=0.45, body=0.65))
add(B, "Music Box", fm(a=6, r2=7.0, i2=0.12, r3=14.0, i3=0.2, dec=0.3, transpose=12, **bellenv(2.2, 1.0)))
add(B, "Celesta", fm(a=1, r2=4.0, i2=0.18, r3=1.0, i3=0.0, dec=0.25, transpose=12, **bellenv(1.6, 0.8)))
add(B, "Steel Drum", fm(a=2, r2=2.0, i2=0.32, r3=3.5, i3=0.15, dec=0.18, **bellenv(1.0, 0.5)))
add(B, "Crotales", fm(a=1, r2=5.4, i2=0.3, r3=1.0, i3=0.0, dec=0.5, transpose=24, **bellenv(3.0, 2.0)))
add(B, "Church Bell", fm(a=5, r2=2.76, i2=0.55, r3=5.4, i3=0.25, dec=1.0, transpose=-12, send2=0.55, **bellenv(8, 5)), {"revSize": 0.95})
add(B, "Glass Bells", wavep("Glass", "Digital1", 0.3, 19, aA=0.001, aD=2.5, aS=0.0, aR=1.5, fltType="LP12", cutoff=9000, send2=0.4))
add(B, "Digital Bell", wavep("Digital2", "Glass", 0.5, 12, aA=0.001, aD=2.0, aS=0.0, aR=1.2, fltType="Off", send1=0.15))
add(B, "Ring Bell", twin(w1=0, w2=3, pitch=7.0, mix=1.0, scream=0.0, fltType="LP12", cutoff=9000, aD=2.0, aS=0.0, aR=1.2))
add(B, "Toy Piano", fm(a=4, r2=5.0, i2=0.2, r3=1.0, i3=0.0, dec=0.15, transpose=12, **bellenv(0.9, 0.4)))
add(B, "Wood Block", fm(a=1, r2=1.41, i2=0.4, r3=1.0, i3=0.0, dec=0.03, **bellenv(0.15, 0.1)))
add(B, "Gamelan", fm(a=3, r2=2.76, i2=0.35, r3=5.4, i3=0.2, dec=0.4, **bellenv(3.0, 1.5)))
add(B, "Bowl Chime", fm(a=6, r2=2.76, i2=0.1, r3=5.4, i3=0.05, dec=2.0, send2=0.55, **bellenv(10, 6)), {"revSize": 0.95})
add(B, "Bell Pad", fm(a=1, r2=3.5, i2=0.2, r3=1.0, i3=0.0, dec=1.5, aA=0.02, aD=4, aS=0.5, aR=2.5, send2=0.5))
add(B, "Mallet Pluck", pluck(pos=0.5, excite=0.3, decay=0.25, bright=0.3, stiff=0.85, pick=0.2, body=0.5))

# ---- Strings --------------------------------------------------------------------------------
S = "Strings"
def strenv(a=0.25, r=0.8):
    return dict(aA=a, aD=1.0, aS=1.0, aR=r)
add(S, "Full Strings", fx(wavep("Strings", "Cello", 0.35, 0, unison=3, detune=0.22, fltType="LP12", cutoff=5500, lfoPitch=0.06, lfoRate=5.2, lfoDelay=0.5, send2=0.38, **strenv()), "Ensemble", 0.35, 0.4, 0.4))
add(S, "Chamber Strings", wavep("Strings", "Cello", 0.5, 0, unison=2, detune=0.15, fltType="LP12", cutoff=4500, lfoPitch=0.08, lfoRate=5.5, lfoDelay=0.4, send2=0.3, **strenv(0.15, 0.5)))
add(S, "Slow Strings", fx(wavep("Strings", "Strings", 0.0, 12, unison=3, detune=0.25, fltType="LP12", cutoff=4800, send2=0.45, **strenv(1.2, 1.6)), "Ensemble", 0.3, 0.5, 0.5))
add(S, "Tremolo Strings", wavep("Strings", "Cello", 0.3, 0, unison=3, detune=0.2, fltType="LP12", cutoff=5000, lfoAmp=0.7, lfoRate=9.0, lfoWave="Triangle", lfoDelay=0.0, send2=0.35, **strenv(0.05, 0.4)))
add(S, "Spiccato", wavep("Strings", "Cello", 0.3, 0, unison=2, detune=0.2, fltType="LP12", cutoff=6000, aA=0.008, aD=0.22, aS=0.0, aR=0.15, send2=0.32))
add(S, "Octave Strings", wavep("Strings", "Strings", 0.45, 12, unison=3, detune=0.2, fltType="LP12", cutoff=6500, send2=0.4, **strenv()))
add(S, "Violin Solo", wavep("Strings", "Cello", 0.15, 0, voiceMode="Legato", glide=0.04, fltType="LP12", cutoff=7000, lfoPitch=0.12, lfoRate=5.6, lfoDelay=0.35, atVib=0.6, send2=0.3, transpose=12, **strenv(0.08, 0.3)))
add(S, "Cello Solo", wavep("Cello", "Strings", 0.2, 0, voiceMode="Legato", glide=0.05, fltType="LP12", cutoff=4000, lfoPitch=0.1, lfoRate=5.0, lfoDelay=0.4, atVib=0.6, send2=0.3, transpose=-12, **strenv(0.1, 0.4)))
add(S, "Pizzicato", pluck(pos=0.4, excite=0.45, decay=0.25, bright=0.45, stiff=0.1, pick=0.15, body=0.85, send2=0.35))
add(S, "String Machine", poly6(wav=0, ens=0.95, drift=0.2, fltType="LP12", cutoff=4200, aA=0.12, aR=0.9, send2=0.3))
add(S, "Analog Strings", fx(analog(SAW, SAW, 0.5, 0, fine=0.3, unison=2, detune=0.3, fltType="LP24", cutoff=3800, aA=0.3, aR=1.0, send2=0.35), "Ensemble", 0.4, 0.55, 0.6))
add(S, "Bowed String", pluck(pos=0.25, excite=0.35, decay=0.85, bright=0.55, stiff=0.0, pick=0.0, body=0.6, bow=0.75, aA=0.15, aR=0.4, lfoPitch=0.08, lfoRate=5.4, lfoDelay=0.4))
add(S, "Synth Strings", poly6(wav=2, pwmDepth=0.5, pwmRate=0.35, ens=0.8, fltType="LP24", cutoff=5000, aA=0.2, aR=0.8, send2=0.35))
add(S, "Dark Strings", wavep("Cello", "Strings", 0.4, 0, unison=3, detune=0.25, fltType="LP24", cutoff=1800, send2=0.4, **strenv(0.4, 1.0)))
add(S, "Marcato Strings", wavep("Strings", "Brass", 0.15, 0, unison=3, detune=0.2, fltType="LP12", cutoff=3000, fltEnv=0.35, fA=0.02, fD=0.3, fS=0.4, send2=0.32, **strenv(0.02, 0.35)))
add(S, "Hybrid Strings", fx(wavep("Strings", "Saw", 0.45, 12, unison=3, detune=0.3, fltType="LP24", cutoff=5200, fltEnv=0.15, send2=0.4, **strenv(0.2, 1.0)), "Chorus", 0.3, 0.5, 0.35))

# ---- Vocal / Airy ---------------------------------------------------------------------------
V = "Vocal/Airy"
def vox(a, b, m=0.3, **kw):
    p = wavep(a, b, m, 0, unison=3, detune=0.18, fltType="LP12", cutoff=6000, aA=0.35, aD=1, aS=1, aR=1.0,
              lfoPitch=0.05, lfoRate=4.8, lfoDelay=0.6, send2=0.45, e7=0.12)
    p.update(kw)
    return fx(p, "Chorus", 0.25, 0.45, 0.35)
add(V, "Choir Aahs", vox("Aah", "Ooh", 0.25))
add(V, "Choir Oohs", vox("Ooh", "Aah", 0.2))
add(V, "Vox Pad", vox("BrightVox", "Aah", 0.4, aA=0.6))
add(V, "Angels", vox("Eee", "Aah", 0.5, transpose=12, e7=0.2, send1=0.2))
add(V, "Breathy Pad", vox("Ooh", "Flute", 0.4, e7=0.5, cutoff=4000))
add(V, "Female Vox", vox("Eee", "BrightVox", 0.35, transpose=12, voiceMode="Poly"))
add(V, "Male Choir", vox("Ooh", "Aah", 0.6, transpose=-12, cutoff=3500))
add(V, "Morph Choir", vox("Ooh", "Eee", 0.0, e4=0.9, fA=1.5, fD=3.0, fS=0.5, aA=0.4))
add(V, "Air Pad", analog(SAW, SAW, 0.5, 12, noise=0.6, fltType="BP", cutoff=2200, reso=0.5, aA=0.9, aR=1.8, lfoFilter=0.25, lfoRate=0.15, lfoDelay=0.0, send2=0.55))
add(V, "Glass Air", wavep("Glass", "Eee", 0.5, 12, e7=0.4, fltType="LP12", cutoff=7000, aA=0.8, aR=2.0, unison=2, detune=0.2, send2=0.5, send1=0.2))
add(V, "Whisper", analog(SINE, SINE, 0.5, 0, noise=1.0, fltType="BP", cutoff=1600, reso=0.65, fltKey=1.0, aA=0.2, aR=0.8, lfoFilter=0.15, lfoRate=0.3, lfoDelay=0.0, send2=0.5))
add(V, "Doo Vox", vox("Ooh", "Bass", 0.3, aA=0.02, aD=0.6, aS=0.5, aR=0.3))
add(V, "Ethereal Voices", vox("Aah", "Glass", 0.45, aA=1.2, aR=2.5, send1=0.25))
add(V, "Gregorian", vox("Ooh", "Aah", 0.35, transpose=-12, unison=4, detune=0.1, cutoff=3000, send2=0.6))
add(V, "Pop Vox", vox("BrightVox", "Eee", 0.3, aA=0.05, aR=0.5))
add(V, "Breath Flute Air", wavep("Flute", "Ooh", 0.3, 12, e6=0.3, e7=0.7, fltType="LP12", cutoff=6500, aA=0.25, aR=1.2, send2=0.5))

# ---- Brass ----------------------------------------------------------------------------------
BR = "Brass"
def sbrass(**kw):
    p = analog(SAW, SAW, 0.5, 0, fine=0.18, fltType="LP24", cutoff=900, reso=0.15, fltEnv=0.5, fA=0.06, fD=0.6,
               fS=0.55, fR=0.3, aA=0.03, aD=0.8, aS=0.9, aR=0.25, send2=0.25)
    p.update(kw)
    return p
add(BR, "Synth Brass", sbrass())
add(BR, "Poly Brass", poly6(wav=0, ens=0.5, fltType="LP24", cutoff=900, fltEnv=0.5, fA=0.07, fD=0.6, fS=0.5, aA=0.04, aR=0.3, send2=0.25))
add(BR, "Brass Stab", sbrass(fA=0.005, fD=0.2, fS=0.2, aD=0.35, aS=0.0, aR=0.12))
add(BR, "Jump Brass", sbrass(e2=SQR, e4=interval(12), cutoff=1400, fltEnv=0.4))
add(BR, "Brass Swell", sbrass(fA=0.8, fD=1.5, aA=0.6, aR=0.8))
add(BR, "Fat Brass", sbrass(unison=3, detune=0.3, cutoff=1100))
add(BR, "Section Brass", wavep("Brass", "Saw", 0.25, 0, unison=3, detune=0.2, fltType="LP12", cutoff=1200, fltEnv=0.45, fA=0.05, fD=0.5, fS=0.6, aA=0.03, aR=0.3, send2=0.3))
add(BR, "Trumpet", wavep("Brass", "Saw", 0.15, 0, voiceMode="Legato", glide=0.03, fltType="LP12", cutoff=1600, fltEnv=0.45, fA=0.04, fD=0.4, fS=0.6, aA=0.02, aR=0.18, lfoPitch=0.1, lfoRate=5.5, lfoDelay=0.4, atVib=0.5, send2=0.25))
add(BR, "Trombone", wavep("Brass", "Cello", 0.25, 0, voiceMode="Legato", glide=0.08, transpose=-12, fltType="LP12", cutoff=900, fltEnv=0.4, fA=0.06, fD=0.5, fS=0.6, aA=0.04, aR=0.2, send2=0.25))
add(BR, "French Horns", wavep("Brass", "Ooh", 0.35, 0, unison=2, detune=0.15, fltType="LP12", cutoff=700, fltEnv=0.35, fA=0.12, fD=0.8, fS=0.6, aA=0.1, aR=0.5, send2=0.42))
add(BR, "Tuba", wavep("Brass", "Bass", 0.4, 0, voiceMode="Legato", transpose=-24, fltType="LP12", cutoff=500, fltEnv=0.3, fA=0.05, aA=0.04, aR=0.15))
add(BR, "FM Brass", fm(a=1, r2=1.0, i2=0.45, r3=1.0, i3=0.3, fb=0.35, dec=1.2, vel=0.6, aA=0.04, aR=0.25, fltType="LP12", cutoff=6000))
add(BR, "Horn Section", fx(wavep("Brass", "Reed", 0.35, 0, unison=3, detune=0.15, fltType="LP12", cutoff=1500, fltEnv=0.4, fA=0.03, fD=0.4, fS=0.55, aA=0.02, aR=0.25, send2=0.25), "Compressor", 0.5, 0.6, 1.0))
add(BR, "Twin Brass", twin(w1=1, w2=0, pitch=0.05, mix=0.5, scream=0.3, cutoff=1100, reso=0.25, fltEnv=0.45, fA=0.05, fD=0.5, fS=0.5, aA=0.03, aR=0.25))
add(BR, "Analog Horns", sbrass(e1=SQR, e2=SAW, cutoff=700, fltEnv=0.55, fA=0.1))

# ---- Woodwind / Reed ------------------------------------------------------------------------
W = "Woodwind/Reed"
def wind(a, b="Sine", m=0.2, **kw):
    p = wavep(a, b, m, 0, voiceMode="Legato", glide=0.03, fltType="LP12", cutoff=6000, aA=0.04, aR=0.2,
              lfoPitch=0.08, lfoRate=5.2, lfoDelay=0.45, atVib=0.5, wheelVib=0.5, send2=0.3, e6=0.2, e7=0.15)
    p.update(kw)
    return p
add(W, "Flute", wind("Flute", "Sine", 0.2, e6=0.35, e7=0.3))
add(W, "Pan Flute", wind("Flute", "Ooh", 0.3, e6=0.7, e7=0.6, voiceMode="Poly", send2=0.45))
add(W, "Clarinet", wind("Clarinet", "Square", 0.15, e6=0.1, e7=0.05))
add(W, "Oboe", wind("Oboe", "Reed", 0.2, e7=0.05, cutoff=5000))
add(W, "Bassoon", wind("Reed", "Oboe", 0.3, transpose=-12, cutoff=2200))
add(W, "Alto Sax", fx(wind("Reed", "Brass", 0.35, cutoff=3500, fltEnv=0.25, fA=0.04, fD=0.4, fS=0.6), "Overdrive", 0.2, 0.6, 0.35))
add(W, "Tenor Sax", fx(wind("Reed", "Brass", 0.45, transpose=-12, cutoff=2600, fltEnv=0.3, fA=0.04, fD=0.5, fS=0.6), "Overdrive", 0.3, 0.5, 0.4))
add(W, "Harmonica", fx(wind("Reed", "Eee", 0.4, cutoff=4500, lfoPitch=0.05), "Amp Sim", 0.25, 0.6, 0.5))
add(W, "Accordion", wavep("Reed", "Reed", 0.0, 12, unison=2, detune=0.35, fltType="LP12", cutoff=5500, aA=0.03, aR=0.15, send2=0.25))
add(W, "Musette", wavep("Reed", "Reed", 0.3, 0, unison=3, detune=0.6, fltType="LP12", cutoff=6000, aA=0.03, aR=0.15, send2=0.25))
add(W, "Recorder", wind("Flute", "Triangle", 0.3, e6=0.25, e7=0.1, transpose=12))
add(W, "Shakuhachi", wind("Flute", "Ooh", 0.25, e6=0.8, e7=0.85, lfoPitch=0.18, lfoRate=4.0, send2=0.5))
add(W, "FM Clarinet", fm(a=1, r2=2.0, i2=0.35, r3=1.0, i3=0.0, dec=2.0, vel=0.3, voiceMode="Legato", aA=0.03, aR=0.15, lfoPitch=0.06, lfoRate=5, lfoDelay=0.4))
add(W, "Breathy Reed", wind("Reed", "Flute", 0.5, e7=0.45, cutoff=3000))
add(W, "Bagpipe Drone", twin(w1=2, w2=2, pitch=-12.0, mix=0.4, scream=0.1, pw=0.6, fltType="LP12", cutoff=3500, aA=0.05, aR=0.2))
add(W, "Ocarina", wind("Sine", "Flute", 0.35, e6=0.4, e7=0.25))

# ---- Guitar / Plucked -----------------------------------------------------------------------
G = "Guitar/Plucked"
add(G, "Nylon Guitar", pluck(pos=0.3, excite=0.5, decay=0.55, bright=0.55, body=0.65, pick=0.2, send2=0.25))
add(G, "Steel Guitar", pluck(pos=0.18, excite=0.85, decay=0.65, bright=0.85, body=0.5, pick=0.35))
add(G, "12-String", fx(pluck(pos=0.15, excite=0.9, decay=0.65, bright=0.88, body=0.45, pick=0.4), "Chorus", 0.35, 0.65, 0.55))
add(G, "Jazz Guitar", pluck(pos=0.35, excite=0.35, decay=0.5, bright=0.35, body=0.75, pick=0.15))
add(G, "Muted Guitar", pluck(pos=0.2, excite=0.6, decay=0.12, bright=0.4, body=0.5, pick=0.4, aR=0.08))
add(G, "Clean Electric", fx(pluck(pos=0.22, excite=0.7, decay=0.6, bright=0.7, body=0.0, pick=0.3), "Chorus", 0.3, 0.45, 0.4))
add(G, "Crunch Guitar", fx(pluck(pos=0.2, excite=0.8, decay=0.65, bright=0.7, body=0.0, pick=0.35), "Amp Sim", 0.5, 0.55, 1.0))
add(G, "Lead Guitar", fx(pluck(pos=0.2, excite=0.85, decay=0.9, bright=0.75, body=0.0, pick=0.3, bow=0.25, voiceMode="Legato", glide=0.04, lfoPitch=0.0, wheelVib=0.6), "Amp Sim", 0.85, 0.5, 1.0), {"dlySync": "1/4"})
add(G, "Power Chords", fx(pluck(pos=0.2, excite=0.9, decay=0.7, bright=0.65, body=0.0, pick=0.4, aR=0.15), "Amp Sim", 0.75, 0.45, 1.0))
add(G, "Concert Harp", pluck(pos=0.5, excite=0.45, decay=0.7, bright=0.5, body=0.25, pick=0.05, send2=0.45))
add(G, "Koto", pluck(pos=0.1, excite=0.75, decay=0.5, bright=0.8, stiff=0.3, body=0.15, pick=0.45, wheelVib=0.6))
add(G, "Sitar", fx(pluck(pos=0.05, excite=0.95, decay=0.7, bright=0.9, stiff=1.0, body=0.3, pick=0.5), "Overdrive", 0.15, 0.8, 0.35))
add(G, "Banjo", pluck(pos=0.08, excite=0.9, decay=0.35, bright=0.85, stiff=0.4, body=0.85, pick=0.6))
add(G, "Mandolin", pluck(pos=0.1, excite=0.85, decay=0.4, bright=0.82, stiff=0.2, body=0.6, pick=0.5))
add(G, "Dulcimer", pluck(pos=0.12, excite=0.8, decay=0.75, bright=0.75, stiff=0.5, body=0.4, pick=0.25))
add(G, "Oud", pluck(pos=0.25, excite=0.6, decay=0.45, bright=0.5, stiff=0.2, body=0.8, pick=0.35))
add(G, "Shamisen", pluck(pos=0.04, excite=1.0, decay=0.3, bright=0.9, stiff=0.6, body=0.5, pick=0.8))
add(G, "Funk Guitar", fx(pluck(pos=0.12, excite=0.85, decay=0.2, bright=0.8, body=0.0, pick=0.45, aR=0.06), "Auto Wah", 0.7, 0.65, 0.9))
add(G, "Ambient Guitar", fx(pluck(pos=0.25, excite=0.6, decay=0.85, bright=0.6, body=0.1, pick=0.2, send1=0.35, send2=0.55), "Chorus", 0.25, 0.6, 0.5), {"dlySync": "1/4D", "dlyFb": 0.5, "revSize": 0.9})

# ---- Bass -----------------------------------------------------------------------------------
BA = "Bass"
def sbass(**kw):
    p = analog(SAW, SQR, 0.4, 0, fine=0.05, sub=0.5, fltType="Ladder", cutoff=300, reso=0.25, fltEnv=0.45,
               fA=0.001, fD=0.28, fS=0.15, fR=0.15, aA=0.002, aD=0.6, aS=0.85, aR=0.08, voiceMode="Mono",
               glide=0.0, transpose=-12, send2=0.04, wheelVib=0.0, wheelCut=0.5)
    p.update(kw)
    return p
add(BA, "Classic Synth Bass", sbass())
add(BA, "Sub Bass", sbass(e1=SINE, e2=SINE, e6=0.7, cutoff=600, fltEnv=0.0, reso=0.0))
add(BA, "Acid Bass", sbass(e1=SAW, e5=0.0, e6=0.0, cutoff=250, reso=0.82, fltEnv=0.65, fD=0.2, fS=0.05, voiceMode="Legato", glide=0.07, drive=0.4, transpose=0))
add(BA, "Reese Bass", sbass(e1=SAW, e2=SAW, e5=0.5, e3=0.6, unison=2, detune=0.45, cutoff=900, fltEnv=0.1, reso=0.1, aS=1.0))
add(BA, "Pluck Bass", sbass(cutoff=200, fltEnv=0.7, fD=0.15, fS=0.0, aD=0.4, aS=0.0, aR=0.1))
add(BA, "Fat Bass", sbass(unison=2, detune=0.2, e6=0.8, cutoff=400, drive=0.3))
add(BA, "Square Bass", sbass(e1=SQR, e2=SQR, e4=interval(-12), e5=0.4, e6=0.0, cutoff=700, fltEnv=0.3))
add(BA, "Wobble Bass", sbass(e1=SAW, e2=SAW, e4=interval(-12), cutoff=350, reso=0.45, fltEnv=0.1, lfoFilter=0.6, lfoRate=3.0, lfoWave="Triangle", lfoDelay=0.0, drive=0.4, aS=1.0))
add(BA, "FM Bass", fm(a=1, r2=1.0, i2=0.4, r3=1.0, i3=0.0, fb=0.2, dec=0.12, vel=0.8, voiceMode="Mono", transpose=-12, aD=1.0, aS=0.6, aR=0.06, send2=0.04))
add(BA, "Slap Bass", fm(a=2, r2=1.0, i2=0.55, r3=3.0, i3=0.25, dec=0.05, vel=0.9, voiceMode="Mono", transpose=-12, aD=0.8, aS=0.35, aR=0.06, send2=0.04))
add(BA, "Finger Bass", pluck(pos=0.3, excite=0.4, decay=0.5, bright=0.35, body=0.35, pick=0.1, voiceMode="Mono", transpose=-12, aR=0.08, send2=0.04))
add(BA, "Pick Bass", fx(pluck(pos=0.15, excite=0.7, decay=0.5, bright=0.55, body=0.3, pick=0.4, voiceMode="Mono", transpose=-12, aR=0.08, send2=0.04), "Compressor", 0.6, 0.6, 1.0))
add(BA, "Fretless Bass", wavep("Bass", "Cello", 0.3, 0, voiceMode="Legato", glide=0.09, transpose=-12, fltType="LP12", cutoff=1500, fltEnv=0.2, aA=0.01, aR=0.12, send2=0.06, wheelVib=0.5))
add(BA, "Upright Bass", pluck(pos=0.35, excite=0.35, decay=0.45, bright=0.3, body=0.9, pick=0.15, voiceMode="Mono", transpose=-12, aR=0.12, send2=0.1))
add(BA, "Poly Bass", poly6(wav=0, sub=0.6, fltType="LP24", cutoff=350, reso=0.2, fltEnv=0.45, fD=0.3, fS=0.2, aR=0.08, transpose=-12, send2=0.04))
add(BA, "Twin Bass", twin(w1=1, w2=1, pitch=-12.0, mix=0.5, scream=0.5, cutoff=400, reso=0.5, fltEnv=0.5, fD=0.25, fS=0.1, aR=0.08, voiceMode="Mono", transpose=-12, send2=0.03))
add(BA, "Digital Bass", wavep("Digital1", "Bass", 0.6, 0, voiceMode="Mono", transpose=-12, fltType="LP24", cutoff=900, fltEnv=0.35, fD=0.3, fS=0.2, aR=0.08, send2=0.04))
add(BA, "Deep House Bass", sbass(e1=SINE, e2=SQR, e5=0.25, e6=0.3, cutoff=500, fltEnv=0.35, fD=0.2, aD=0.5, aS=0.3))

# ---- Slow Synth (pads) ----------------------------------------------------------------------
SL = "Slow Synth"
def pad(**kw):
    p = analog(SAW, SAW, 0.5, 0, fine=0.3, unison=2, detune=0.3, fltType="LP24", cutoff=2200, reso=0.15,
               fltEnv=0.15, fA=1.2, fD=2.5, fS=0.6, fR=1.5, aA=0.8, aD=2, aS=1.0, aR=1.8, send2=0.45, send1=0.1)
    p.update(kw)
    return fx(p, "Chorus", 0.2, 0.55, 0.4)
add(SL, "Warm Pad", pad())
add(SL, "Sweep Pad", pad(cutoff=600, fltEnv=0.55, fA=2.5, fD=4.0, fS=0.3, reso=0.35))
add(SL, "Glass Pad", wavep("Glass", "Digital1", 0.4, 12, unison=3, detune=0.25, fltType="LP12", cutoff=5000, aA=0.9, aR=2.0, send2=0.5, send1=0.2))
add(SL, "PWM Pad", poly6(wav=2, pwmDepth=0.8, pwmRate=0.3, ens=0.7, fltType="LP24", cutoff=2500, aA=0.7, aR=1.6, send2=0.45))
add(SL, "Dark Pad", pad(cutoff=700, e1=SAW, e2=SQR, fltEnv=0.1))
add(SL, "Dream Pad", pad(cutoff=3500, send1=0.35, send2=0.6, e7=0.15))
add(SL, "Evolving Wave", wavep("Digital2", "BrightVox", 0.0, 12, e4=1.0, fA=3.0, fD=6.0, fS=0.4, unison=2, detune=0.2, fltType="LP12", cutoff=4500, lfoFilter=0.15, lfoRate=0.12, lfoDelay=0.0, aA=1.0, aR=2.5, send2=0.55))
add(SL, "Space Pad", pad(unison=4, detune=0.45, send1=0.4, send2=0.65, cutoff=2800))
add(SL, "Hollow Pad", pad(e1=SQR, e2=SQR, e4=interval(12), fltType="BP", cutoff=1200, reso=0.4, fltEnv=0.2))
add(SL, "Analog Choir", wavep("Ooh", "Saw", 0.4, 0, unison=3, detune=0.3, fltType="LP24", cutoff=2600, aA=0.9, aR=2.0, send2=0.5))
add(SL, "Poly Pad", poly6(wav=0, ens=0.85, drift=0.5, fltType="LP24", cutoff=1800, fltEnv=0.2, fA=1.0, fD=2.0, aA=0.6, aR=1.5, send2=0.45))
add(SL, "Fifths Pad", pad(e4=interval(7), cutoff=1900))
add(SL, "Soft Sine Pad", pad(e1=SINE, e2=TRI, e4=interval(12), cutoff=4000, fltEnv=0.0))
add(SL, "Twin Pad", twin(w1=1, w2=0, pitch=0.1, mix=0.5, scream=0.05, cutoff=1500, reso=0.35, fltEnv=0.25, fA=1.5, fD=3.0, aA=0.9, aR=1.8, send2=0.5))
add(SL, "FM Pad", fm(a=7, r2=2.0, i2=0.0, r3=4.0, i3=0.0, dec=3.0, aA=0.9, aR=2.0, fltType="LP12", cutoff=4000, send2=0.5, unison=1))
add(SL, "Cinematic Pad", wavep("Strings", "Aah", 0.5, 12, unison=4, detune=0.3, fltType="LP24", cutoff=2400, aA=1.5, aR=3.0, send2=0.65, send1=0.15))

# ---- Fast Synth -----------------------------------------------------------------------------
F = "Fast Synth"
add(F, "Poly Synth", analog(SAW, SAW, 0.5, 0, fine=0.25, cutoff=2400, fltEnv=0.35, fD=0.5, fS=0.4, aR=0.3))
add(F, "80s Poly", poly6(wav=0, ens=0.8, cutoff=2600, fltEnv=0.35, fD=0.5, fS=0.4, aR=0.35))
add(F, "Synth Pluck", analog(SAW, SQR, 0.4, 12, cutoff=900, fltEnv=0.6, fD=0.25, fS=0.0, aD=0.6, aS=0.0, aR=0.25, send1=0.15))
add(F, "Sync Pluck", wavep("Sync", "Saw", 0.0, 0, fltType="LP24", cutoff=1500, fltEnv=0.55, fD=0.3, fS=0.1, aD=0.6, aS=0.0, aR=0.3))
add(F, "Super Saw", fx(analog(SAW, SAW, 0.5, 0, fine=0.4, unison=4, detune=0.55, cutoff=6000, fltEnv=0.1, aR=0.35), "Chorus", 0.25, 0.4, 0.3))
add(F, "Trance Pluck", analog(SAW, SAW, 0.5, 12, fine=0.3, unison=3, detune=0.4, cutoff=700, fltEnv=0.6, fD=0.22, fS=0.0, aD=0.4, aS=0.0, aR=0.2, send1=0.35), {"dlySync": "1/8D", "dlyFb": 0.4})
add(F, "Square Keys", analog(SQR, SQR, 0.5, 12, pw=0.2, cutoff=3000, fltEnv=0.25, fD=0.4, fS=0.5, aR=0.25))
add(F, "Chord Stab", analog(SAW, SAW, 0.5, 0, fine=0.3, unison=2, detune=0.3, cutoff=1500, fltEnv=0.5, fD=0.18, fS=0.0, aD=0.3, aS=0.0, aR=0.15, send2=0.3))
add(F, "Digital Keys", fm(a=3, r2=2.0, i2=0.3, r3=3.0, i3=0.2, dec=0.4, aD=2.0, aS=0.3, aR=0.35))
add(F, "Bell Pluck", fm(a=6, r2=3.5, i2=0.25, r3=7.0, i3=0.1, dec=0.2, aD=1.0, aS=0.0, aR=0.4, send1=0.2))
add(F, "Juno Keys", poly6(wav=1, pw=0.4, ens=0.9, sub=0.3, cutoff=3200, fltEnv=0.3, fD=0.4, fS=0.5, aR=0.3))
add(F, "Brass Keys", sbrass(fA=0.003, fD=0.3, fS=0.4, aA=0.003, cutoff=1500))
add(F, "Glass Keys", wavep("Glass", "Sine", 0.3, 12, fltType="Off", aD=1.5, aS=0.3, aR=0.5, send1=0.15))
add(F, "Twin Stab", twin(w1=1, w2=1, pitch=7.0, mix=0.5, scream=0.4, cutoff=1200, reso=0.45, fltEnv=0.5, fD=0.2, fS=0.1, aD=0.3, aS=0.2, aR=0.15))
add(F, "Hoover", fx(analog(SAW, SAW, 0.6, -12, fine=0.5, unison=4, detune=0.6, cutoff=3000, reso=0.2, glide=0.15, voiceMode="Mono", pitchEg=-0.08, fA=0.0, fD=0.25, fS=0.0), "Chorus", 0.45, 0.8, 0.6))
add(F, "Organ Synth", analog(SQR, SAW, 0.3, 12, cutoff=4000, fltEnv=0.1, aR=0.1))

# ---- Lead Synth -----------------------------------------------------------------------------
L = "Lead Synth"
def lead(**kw):
    p = analog(SAW, SAW, 0.5, 0, fine=0.2, cutoff=2800, reso=0.2, fltEnv=0.3, fD=0.4, fS=0.6, aA=0.004, aR=0.15,
               voiceMode="Legato", glide=0.04, lfoPitch=0.0, wheelVib=0.6, send1=0.2, send2=0.2)
    p.update(kw)
    return p
add(L, "Saw Lead", lead())
add(L, "Square Lead", lead(e1=SQR, e2=SQR, e4=interval(12), pw=0.1, cutoff=3500))
add(L, "Sync Lead", wavep("Sync", "Saw", 0.2, 12, voiceMode="Legato", glide=0.04, fltType="LP24", cutoff=4500, fltEnv=0.2, aR=0.15, wheelVib=0.6, send1=0.2))
add(L, "Fifth Lead", lead(e4=interval(7)))
add(L, "Fat Lead", lead(unison=3, detune=0.35, cutoff=3200))
add(L, "Scream Lead", fx(twin(w1=1, w2=1, pitch=0.1, mix=0.5, scream=0.85, cutoff=1500, reso=0.75, fltEnv=0.35, fD=0.6, voiceMode="Legato", glide=0.05, aR=0.15, wheelVib=0.6, send1=0.25), "Overdrive", 0.3, 0.5, 0.4))
add(L, "Whistle Lead", lead(e1=SINE, e2=SINE, fltType="Off", lfoPitch=0.12, lfoRate=5.5, lfoDelay=0.3, glide=0.08))
add(L, "Pulse Lead", lead(e1=SQR, e2=SQR, e4=interval(0), pw=0.8, cutoff=3000))
add(L, "Portamento Lead", lead(glide=0.25, voiceMode="Mono", e4=interval(-12)))
add(L, "Chip Lead", wavep("Pulse25", "Pulse10", 0.0, 0, voiceMode="Mono", fltType="Off", aR=0.06, lfoPitch=0.0, wheelVib=0.4))
add(L, "FM Lead", fm(a=1, r2=1.0, i2=0.5, r3=2.0, i3=0.25, fb=0.4, dec=1.5, voiceMode="Legato", glide=0.04, aR=0.15, wheelVib=0.6, send1=0.2))
add(L, "Theremin", lead(e1=SINE, e2=TRI, e5=0.2, fltType="Off", glide=0.35, voiceMode="Legato", lfoPitch=0.15, lfoRate=6.0, lfoDelay=0.25, aA=0.08, aR=0.4, send2=0.35))
add(L, "Prog Lead", lead(fltType="Ladder", cutoff=1200, reso=0.6, fltEnv=0.5, fD=0.6, fS=0.4, e2=SQR))
add(L, "Poly Lead", poly6(wav=0, ens=0.3, cutoff=2800, fltEnv=0.3, voiceMode="Legato", glide=0.03, aR=0.15, wheelVib=0.6, send1=0.2))
add(L, "Wave Lead", wavep("BrightVox", "Saw", 0.5, 12, voiceMode="Legato", glide=0.05, fltType="LP12", cutoff=5000, aR=0.2, wheelVib=0.6, send1=0.2))
add(L, "Mini Lead", lead(fltType="Ladder", e1=SAW, e2=SQR, e4=interval(-12), e6=0.3, cutoff=1800, reso=0.35, fltEnv=0.35, drive=0.3))

# ---- Motion Synth ---------------------------------------------------------------------------
M = "Motion Synth"
add(M, "Filter Wobble", analog(SAW, SAW, 0.5, 0, fine=0.3, cutoff=700, reso=0.5, lfoFilter=0.45, lfoRate=2.0, lfoWave="Triangle", lfoDelay=0.0, aR=0.4, send2=0.3))
add(M, "S&H Bubbles", analog(SQR, SAW, 0.4, 12, cutoff=1200, reso=0.65, lfoFilter=0.5, lfoRate=7.0, lfoWave="S&H", lfoDelay=0.0, aR=0.6, send1=0.3, send2=0.35))
add(M, "Pulsing Pad", fx(pad(cutoff=1800), "Tremolo", 0.5, 0.75, 1.0))
add(M, "Phase Sweep", fx(pad(cutoff=3000), "Phaser", 0.1, 0.7, 0.8))
add(M, "Flanged Pad", fx(pad(cutoff=3500), "Flanger", 0.12, 0.75, 0.7))
add(M, "Auto Pan Keys", fx(epiano(e2=0.6), "Auto Pan", 0.4, 0.9, 1.0))
add(M, "Moving Choir", fx(vox("Aah", "Eee", 0.0, e4=1.0, fA=4.0, fD=6.0, fS=0.0), "Auto Pan", 0.12, 0.6, 1.0))
add(M, "Wah Motion", fx(analog(SAW, SQR, 0.5, 0, cutoff=5000, aR=0.4), "Auto Wah", 0.8, 0.8, 1.0))
add(M, "Gated Pad", fx(pad(cutoff=2500), "Tremolo", 0.65, 1.0, 1.0))
add(M, "Resonant Sweep", analog(SAW, SAW, 0.5, 0, fine=0.4, unison=2, detune=0.3, cutoff=400, reso=0.7, lfoFilter=0.55, lfoRate=0.12, lfoWave="Sine", lfoDelay=0.0, aA=0.4, aR=1.5, send2=0.45))
add(M, "Vowel Morph", wavep("Aah", "Eee", 0.0, 0, e4=1.0, fA=0.8, fD=1.2, fS=0.0, fltType="Off", unison=2, detune=0.2, lfoPitch=0.05, aA=0.2, aR=1.0, send2=0.4))
add(M, "Pulse Width Dance", poly6(wav=2, pwmDepth=1.0, pwmRate=0.7, ens=0.5, cutoff=3000, aR=0.6, send2=0.35))
add(M, "Rhythmic Wave", fx(wavep("Digital1", "Digital2", 0.5, 12, fltType="LP24", cutoff=2500, reso=0.35, lfoFilter=0.35, lfoRate=4.0, lfoWave="Square", lfoDelay=0.0, aR=0.5, send1=0.3), "Delay", 0.45, 0.5, 0.35))
add(M, "Rotor Synth", fx(analog(SAW, SQR, 0.5, 12, cutoff=3000, aR=0.4), "Rotary", 0.8, 0.3, 1.0))
add(M, "FM Motion", fm(a=1, r2=2.0, i2=0.4, r3=1.0, i3=0.0, dec=6.0, lfoAmp=0.0, lfoPitch=0.0, aA=0.5, aR=1.5, fltType="LP12", cutoff=3000, lfoFilter=0.3, lfoRate=0.3, lfoDelay=0.0, send2=0.4))
add(M, "Twin Sweep", twin(w1=1, w2=2, pitch=0.2, mix=0.5, hpf=0.3, peak=0.6, scream=0.3, cutoff=800, reso=0.6, lfoFilter=0.45, lfoRate=0.5, lfoDelay=0.0, aR=0.8, send2=0.35))

# ---- SE -------------------------------------------------------------------------------------
X = "SE"
add(X, "Laser Zap", analog(SAW, SQR, 0.5, 12, cutoff=5000, pitchEg=-0.9, fA=0.001, fD=0.25, fS=0.0, aD=0.3, aS=0.0, aR=0.1, fltEnv=0.0))
add(X, "Riser", analog(SAW, SAW, 0.5, 7, noise=0.4, unison=3, detune=0.4, pitchEg=0.5, fA=6.0, fD=1.0, fS=1.0, aA=4.0, aR=1.0, cutoff=800, fltEnv=0.0, lfoFilter=0.0, send2=0.5, send1=0.3))
add(X, "Downer", analog(SAW, SAW, 0.5, 0, noise=0.3, pitchEg=-0.7, fA=0.001, fD=3.0, fS=0.0, aD=3, aS=0.0, aR=1.0, cutoff=3000, send2=0.4))
add(X, "Siren", analog(SQR, SQR, 0.5, 12, cutoff=3000, lfoPitch=1.0, lfoRate=0.35, lfoWave="Triangle", lfoDelay=0.0, voiceMode="Mono"))
add(X, "Alarm", analog(SQR, SQR, 0.5, 0, cutoff=2500, lfoPitch=1.0, lfoRate=3.0, lfoWave="Square", lfoDelay=0.0, voiceMode="Mono"))
add(X, "UFO", analog(SINE, TRI, 0.5, 7, cutoff=1500, reso=0.7, lfoPitch=0.6, lfoRate=8.0, lfoWave="Sine", lfoDelay=0.0, lfoFilter=0.3, send2=0.45, send1=0.3))
add(X, "Bubbles", analog(SINE, SINE, 0.5, 12, cutoff=4000, lfoPitch=1.0, lfoRate=9.0, lfoWave="S&H", lfoDelay=0.0, aR=0.8, send2=0.4))
add(X, "Thunder", analog(SAW, SAW, 0.5, 0, noise=1.0, e5=0.0, fltType="LP24", cutoff=180, reso=0.3, fltEnv=0.6, fA=0.05, fD=3.0, fS=0.1, aA=0.05, aD=4.0, aS=0.0, aR=2.5, transpose=-24, send2=0.6))
add(X, "Wind", analog(SINE, SINE, 0.0, 0, noise=1.0, fltType="BP", cutoff=900, reso=0.75, fltKey=0.8, lfoFilter=0.55, lfoRate=0.15, lfoDelay=0.0, aA=1.2, aR=2.5, send2=0.5))
add(X, "Ocean Waves", analog(SINE, SINE, 0.0, 0, noise=1.0, fltType="LP12", cutoff=1200, lfoFilter=0.6, lfoAmp=0.7, lfoRate=0.12, lfoDelay=0.0, aA=2.0, aR=3.0, send2=0.55))
add(X, "Rain", fx(drums(e5=0.1, e8=1.0, send2=0.6), "Lo-Fi", 0.2, 0.0, 0.0))
add(X, "Radio Noise", fx(analog(SAW, SAW, 0.5, 0, noise=0.8, fltType="BP", cutoff=1800, reso=0.5, lfoFilter=0.3, lfoRate=6.0, lfoWave="S&H", lfoDelay=0.0), "Lo-Fi", 0.75, 0.6, 1.0))
add(X, "Robot Voice", twin(w1=1, w2=3, pitch=5.0, mix=0.8, scream=0.3, fltType="BP", cutoff=1500, reso=0.6, voiceMode="Mono"))
add(X, "Metal Hit", fm(a=5, r2=7.13, i2=0.8, r3=2.76, i3=0.7, fb=0.8, dec=0.4, aD=2.5, aS=0.0, aR=1.5, send2=0.5))
add(X, "Glitch", fx(analog(SQR, SAW, 0.5, 19, cutoff=4000, lfoPitch=1.0, lfoRate=15.0, lfoWave="S&H", lfoDelay=0.0, aD=0.2, aS=0.0, aR=0.1), "Lo-Fi", 0.6, 0.7, 1.0))
add(X, "Space Drone", fx(analog(SAW, SAW, 0.5, 7, fine=0.5, unison=4, detune=0.6, cutoff=600, reso=0.5, lfoFilter=0.4, lfoRate=0.08, lfoDelay=0.0, aA=3.0, aR=4.0, transpose=-12, send2=0.7, send1=0.35), "Phaser", 0.05, 0.6, 0.6))
add(X, "Dark Drone", fm(a=1, r2=0.5, i2=0.5, r3=1.41, i3=0.3, fb=0.5, dec=6.0, aA=2.0, aR=4.0, transpose=-24, fltType="LP24", cutoff=900, send2=0.6))
add(X, "Heartbeat", drums(e1=0.0, e2=0.25, e7=0.0, send2=0.3))
add(X, "Telephone", fx(wavep("BrightVox", "Square", 0.5, 0, fltType="BP", cutoff=1800, reso=0.3, aR=0.2), "Lo-Fi", 0.6, 0.4, 1.0))

# ---- Hit / Arpg -----------------------------------------------------------------------------
H = "Hit/Arpg"
ARP = {"arpOn": True, "arpMode": "Up", "arpRate": "1/16", "arpOct": 2, "arpGate": 0.45}
add(H, "Orchestra Hit", wavep("Strings", "Brass", 0.5, 12, unison=4, detune=0.3, fltType="LP12", cutoff=6000, aA=0.002, aD=0.6, aS=0.0, aR=0.4, send2=0.5))
add(H, "Synth Hit", analog(SAW, SAW, 0.5, 7, unison=3, detune=0.4, cutoff=2000, fltEnv=0.5, fD=0.2, fS=0.0, aD=0.5, aS=0.0, aR=0.3, send2=0.45))
add(H, "Brass Hit", sbrass(fA=0.002, fD=0.25, fS=0.0, aD=0.45, aS=0.0, aR=0.3, unison=3, detune=0.25, send2=0.45))
add(H, "Choir Hit", vox("Aah", "Brass", 0.3, aA=0.003, aD=0.7, aS=0.0, aR=0.4))
add(H, "Rave Stab", fx(analog(SAW, SQR, 0.5, 12, unison=4, detune=0.5, cutoff=2500, fltEnv=0.4, fD=0.2, fS=0.0, aD=0.35, aS=0.0, aR=0.2), "Overdrive", 0.4, 0.6, 0.5))
add(H, "Pluck Arp", analog(SAW, SQR, 0.4, 12, cutoff=800, fltEnv=0.6, fD=0.18, fS=0.0, aD=0.4, aS=0.0, aR=0.2, send1=0.3), dict(ARP, dlySync="1/8D", dlyFb=0.35))
add(H, "Arp Bells", fm(a=6, r2=3.5, i2=0.2, r3=7.0, i3=0.1, dec=0.2, aD=1.0, aS=0.0, aR=0.6, send1=0.3, send2=0.4), dict(ARP, arpMode="Up-Down", arpOct=3, dlySync="1/8D"))
add(H, "Arp Bass", sbass(cutoff=300, fltEnv=0.6, fD=0.15, fS=0.0, voiceMode="Poly"), dict(ARP, arpRate="1/16", arpOct=1, arpGate=0.6))
add(H, "Trance Gate", analog(SAW, SAW, 0.5, 0, unison=4, detune=0.5, cutoff=4000, aR=0.05, send2=0.35), dict(ARP, arpMode="Chord", arpRate="1/16", arpGate=0.5))
add(H, "Random Blips", analog(SQR, SINE, 0.3, 19, cutoff=3000, fltEnv=0.4, fD=0.1, fS=0.0, aD=0.2, aS=0.0, aR=0.1, send1=0.4), dict(ARP, arpMode="Random", arpRate="1/16", arpOct=3, arpGate=0.3, dlySync="1/8"))
add(H, "Chord Arp", poly6(wav=0, ens=0.6, cutoff=2000, fltEnv=0.4, fD=0.25, fS=0.1, aR=0.2, send1=0.25), dict(ARP, arpMode="Chord", arpRate="1/8", arpGate=0.4, dlySync="1/8D"))
add(H, "Sequence Pluck", wavep("Sync", "Saw", 0.0, 0, fltType="LP24", cutoff=1200, fltEnv=0.55, fD=0.15, fS=0.0, aD=0.3, aS=0.0, aR=0.15, send1=0.3), dict(ARP, arpMode="Up-Down", arpRate="1/16", arpOct=2, dlySync="1/8D"))
add(H, "Harp Arp", pluck(pos=0.5, excite=0.5, decay=0.65, bright=0.55, body=0.25, pick=0.05, send2=0.45), dict(ARP, arpMode="Up", arpRate="1/16T", arpOct=3, arpGate=0.9))
add(H, "Glass Arp", wavep("Glass", "Digital1", 0.3, 12, fltType="Off", aD=0.8, aS=0.0, aR=0.5, send1=0.35, send2=0.4), dict(ARP, arpMode="Up", arpRate="1/16", arpOct=3, dlySync="1/4"))
add(H, "Twin Seq", twin(w1=1, w2=1, pitch=0.1, scream=0.5, cutoff=600, reso=0.7, fltEnv=0.6, fD=0.15, fS=0.0, aR=0.08, voiceMode="Poly"), dict(ARP, arpRate="1/16", arpOct=1, arpGate=0.35, arpSwing=0.2))
add(H, "Orchestral Stab", wavep("Brass", "Strings", 0.5, 0, unison=4, detune=0.3, fltType="LP12", cutoff=4000, fltEnv=0.3, fD=0.2, aA=0.003, aD=0.4, aS=0.0, aR=0.3, send2=0.5))

# ---- Drums ----------------------------------------------------------------------------------
D = "Drums"
add(D, "Standard Kit", drums(e7=0.6))
add(D, "Electro Kit", drums(e1=0.25, e2=0.75, e3=0.45, e4=0.4, e5=0.3, e7=0.0, e8=0.2))
add(D, "Dance Kit", drums(e1=0.4, e2=0.5, e3=0.6, e4=0.85, e5=0.3, e7=0.2, e8=0.6))
add(D, "Rock Kit", fx(drums(e1=0.5, e2=0.35, e3=0.4, e4=0.6, e7=0.85, e8=0.7, send2=0.25), "Compressor", 0.65, 0.6, 1.0), {"revSize": 0.45})
add(D, "Jazz Kit", drums(e1=0.7, e2=0.25, e3=0.75, e4=0.35, e5=0.6, e6=0.75, e7=0.95, e8=0.6, send2=0.2), {"revSize": 0.4})
add(D, "Brush Kit", drums(e1=0.6, e2=0.2, e3=0.85, e4=1.0, e5=0.7, e7=1.0, e8=0.9, ampVel=0.9))
add(D, "Trap Kit", drums(e1=0.1, e2=1.0, e3=0.55, e4=0.75, e5=0.15, e7=0.05, e8=0.5))
add(D, "Techno Kit", fx(drums(e1=0.35, e2=0.55, e3=0.35, e4=0.6, e5=0.2, e7=0.15, e8=0.4), "Overdrive", 0.3, 0.5, 0.5))
add(D, "House Kit", drums(e1=0.45, e2=0.45, e3=0.55, e4=0.7, e5=0.45, e7=0.25, e8=0.65))
add(D, "Industrial Kit", fx(drums(e1=0.3, e2=0.6, e3=0.3, e4=0.9, e7=0.5, e8=0.2), "Amp Sim", 0.6, 0.4, 0.8))
add(D, "Lo-Fi Kit", fx(drums(e1=0.45, e2=0.4, e3=0.5, e7=0.7), "Lo-Fi", 0.55, 0.35, 0.8))
add(D, "Vintage Kit", fx(drums(e1=0.55, e2=0.35, e3=0.6, e7=0.8), "Tone EQ", 0.6, 0.3, 1.0))
add(D, "Percussion Kit", drums(e1=0.6, e6=0.95, e7=0.7, e5=0.55))
add(D, "Hip-Hop Kit", fx(drums(e1=0.3, e2=0.65, e3=0.45, e4=0.5, e7=0.55, e8=0.4), "Compressor", 0.55, 0.5, 1.0))
add(D, "Big Room Kit", drums(e1=0.35, e2=0.7, e4=0.8, e7=0.3, send2=0.4), {"revSize": 0.85})
add(D, "Minimal Kit", drums(e1=0.5, e2=0.3, e3=0.65, e4=0.3, e5=0.15, e7=0.1, e8=0.3))

# flavour plans: the order in which flavours are tried for each category
PLAN = {
    "Keyboard": ["Hall", "Room", "Dry", "Bright", "Dark", "Chorus", "Echo"],
    "Organ": ["Fast Rotor", "Dirty", "Perc", "Hall", "Room", "Dry"],
    "Bell/Mallet": ["Hall", "Echo", "Space", "Dry", "Long", "Chorus"],
    "Strings": ["Hall", "Slow", "Dark", "Bright", "Room", "Wide"],
    "Vocal/Airy": ["Space", "Hall", "Dark", "Slow", "Bright", "Wide"],
    "Brass": ["Hall", "Bright", "Dark", "Stack", "Soft", "Room"],
    "Woodwind/Reed": ["Hall", "Room", "Dark", "Bright", "Echo", "Dry"],
    "Guitar/Plucked": ["Hall", "Chorus", "Echo", "Dry", "Room", "Phaser"],
    "Bass": ["Glide", "Drive", "Dark", "Bright", "Comp", "Stack"],
    "Slow Synth": ["Space", "Motion", "Dark", "Bright", "Stack", "Phaser"],
    "Fast Synth": ["Echo", "Bright", "Dark", "Stack", "Drive", "Wide"],
    "Lead Synth": ["Echo", "Drive", "Glide", "Dark", "Stack", "Bright"],
    "Motion Synth": ["Fast", "Space", "Dark", "Bright", "Echo", "Wide"],
    "SE": ["Space", "Echo", "Dark", "Lo-Fi", "Hall", "8vb"],
    "Hit/Arpg": ["Hall", "Echo", "UpDown", "Random", "Triplet", "Dark"],
    "Drums": ["Room", "Hall", "Tight", "Deep", "Comp", "Lo-Fi"],
}
TARGET_PER_CAT = 64


def build_programs():
    programs = []
    names = set()
    for cat in PROGRAM_CATS:
        recipes = R[cat]
        out = []
        # base presets first
        for name, p, g in recipes:
            out.append((name, copy.deepcopy(p), dict(g)))
        # then flavours, round-robin over recipes
        plan = PLAN[cat]
        k = 0
        while len(out) < TARGET_PER_CAT and k < len(plan) * len(recipes) * 2:
            f = plan[(k // len(recipes)) % len(plan)]
            name, p, g = recipes[k % len(recipes)]
            k += 1
            nm = "%s %s" % (name, f)
            if nm in names or any(o[0] == nm for o in out):
                continue
            # skip flavours that do nothing useful for this recipe
            if f in ("Glide", "Mono") and p.get("voiceMode") == "Legato":
                continue
            if f in ("Arp", "UpDown", "Random", "Triplet") and not g.get("arpOn"):
                continue
            if f in ("Fast Rotor", "Dirty", "Perc") and p["engine"] != "Tonewheel Organ":
                continue
            p2, g2 = copy.deepcopy(p), dict(g)
            FLAVOURS[f](p2, g2)
            if p2 == p and g2 == g:
                continue
            out.append((nm, p2, g2))
        for name, p, g in out:
            assert name not in names, name
            names.add(name)
            programs.append({"name": name, "cat": cat, "p": p, "g": g})
    return programs


# ---------------------------------------------------------------------------------------------
# Combis
def T(prog, **over):
    return {"prog": prog, "p": over}


def build_combis(names):
    def chk(n):
        assert n in names, "combi references unknown program " + n
        return n

    C = []

    def combi(cat, name, timbres, g=None):
        for t in timbres:
            if t is not None:
                chk(t["prog"])
        C.append({"name": name, "cat": cat, "t": timbres, "g": g or {}})

    SPLIT = 59  # B3: left hand below C4
    lo = dict(keyHi=SPLIT)
    hi = dict(keyLo=SPLIT + 1)

    # Keyboard
    k = "Keyboard"
    combi(k, "Grand & Strings", [T("Concert Grand"), T("Full Strings", level=0.45)])
    combi(k, "Grand & Warm Pad", [T("Concert Grand"), T("Warm Pad", level=0.4)])
    combi(k, "Tine & Pad", [T("Classic Tine"), T("Poly Pad", level=0.4)])
    combi(k, "Piano & Choir", [T("Ballad Grand"), T("Choir Oohs", level=0.4)])
    combi(k, "Bass / Grand Split", [T("Upright Bass", **lo), T("Jazz Grand", **hi)])
    combi(k, "EP / Bass Split", [T("Finger Bass", **lo), T("Suitcase Tine", **hi)])
    combi(k, "Grand & Bells", [T("Bright Grand"), T("Glass Bells", level=0.3)])
    combi(k, "Felt & Air", [T("Felt Piano"), T("Air Pad", level=0.35)])
    combi(k, "Dyno & Strings", [T("Dyno Tine"), T("String Machine", level=0.4)])
    combi(k, "Reed & Organ", [T("Reed Piano"), T("Ballad Organ", level=0.35)])

    o = "Organ"
    combi(o, "Organ / Bass Split", [T("Finger Bass", **lo), T("Jazz Organ", **hi)])
    combi(o, "Gospel Layer", [T("Gospel Organ"), T("Choir Aahs", level=0.35)])
    combi(o, "Rock Organ & Piano", [T("Rock Organ"), T("Rock Piano", level=0.5)])
    combi(o, "Cathedral", [T("Pipe Organ Full"), T("Choir Oohs", level=0.35)])
    combi(o, "Organ & Strings", [T("Ballad Organ"), T("Slow Strings", level=0.4)])
    combi(o, "Soul Split", [T("Pick Bass", **lo), T("Soul Organ", **hi)])
    combi(o, "Pipes & Bells", [T("Pipe Flutes"), T("Tubular Bells", level=0.35)])
    combi(o, "Combo Organ & Bass", [T("Square Bass", **lo), T("Combo Organ", **hi)])

    b = "Bell/Mallet"
    combi(b, "Bell Choir", [T("Tubular Bells"), T("Choir Aahs", level=0.45)])
    combi(b, "Celesta & Strings", [T("Celesta"), T("Chamber Strings", level=0.45)])
    combi(b, "Vibes & Bass", [T("Upright Bass", **lo), T("Vibraphone", **hi)])
    combi(b, "Marimba Layer", [T("Marimba"), T("Kalimba", level=0.5)])
    combi(b, "Music Box Dream", [T("Music Box"), T("Dream Pad", level=0.4)])
    combi(b, "Glass Garden", [T("Glass Bells"), T("Glass Pad", level=0.4)])
    combi(b, "Gamelan Orchestra", [T("Gamelan"), T("Steel Drum", level=0.5), T("Bowl Chime", level=0.4)])
    combi(b, "Bell Pad Layer", [T("Bell Pad"), T("Warm Pad", level=0.45)])

    s = "Strings"
    combi(s, "Strings & Horns", [T("Full Strings"), T("French Horns", level=0.5)])
    combi(s, "String Octaves", [T("Chamber Strings"), T("Octave Strings", level=0.5)])
    combi(s, "Cellos / Violins", [T("Cello Solo", **lo), T("Violin Solo", **hi)])
    combi(s, "Strings & Choir", [T("Slow Strings"), T("Choir Aahs", level=0.45)])
    combi(s, "Pizz & Arco", [T("Pizzicato", velHi=90), T("Spiccato", velLo=91)])
    combi(s, "Machine & Analog", [T("String Machine"), T("Analog Strings", level=0.5)])
    combi(s, "Tremolo Ensemble", [T("Tremolo Strings"), T("Dark Strings", level=0.5)])
    combi(s, "Hybrid Orchestra", [T("Hybrid Strings"), T("Cinematic Pad", level=0.45)])

    pd = "Pads"
    combi(pd, "Vector Pads", [T("Warm Pad", vector=True), T("Glass Pad", vector=True), T("PWM Pad", vector=True), T("Choir Aahs", vector=True)])
    combi(pd, "Dream Layers", [T("Dream Pad"), T("Glass Air", level=0.4)])
    combi(pd, "Analog Heaven", [T("Poly Pad"), T("Analog Choir", level=0.5)])
    combi(pd, "Dark Matter", [T("Dark Pad"), T("Space Drone", level=0.35)])
    combi(pd, "Cinema Swell", [T("Cinematic Pad"), T("Slow Strings", level=0.45)])
    combi(pd, "Sweep & Air", [T("Sweep Pad"), T("Air Pad", level=0.4)])
    combi(pd, "Evolving Worlds", [T("Evolving Wave"), T("Ethereal Voices", level=0.45)])
    combi(pd, "Fifth Dimension", [T("Fifths Pad"), T("Soft Sine Pad", level=0.5)])

    br = "Brass/Reed"
    combi(br, "Big Band", [T("Section Brass"), T("Alto Sax", level=0.5)])
    combi(br, "Synth Brass Stack", [T("Synth Brass"), T("Poly Brass", level=0.6)])
    combi(br, "Horns & Strings", [T("French Horns"), T("Full Strings", level=0.45)])
    combi(br, "Tuba / Trumpet", [T("Tuba", **lo), T("Trumpet", **hi)])
    combi(br, "Reed Section", [T("Clarinet"), T("Oboe", level=0.6), T("Bassoon", level=0.6)])
    combi(br, "Brass Swell Pad", [T("Brass Swell"), T("Warm Pad", level=0.4)])
    combi(br, "Sax / Bass", [T("Finger Bass", **lo), T("Tenor Sax", **hi)])
    combi(br, "Funk Horns", [T("Horn Section"), T("Brass Stab", level=0.5)])

    orc = "Orchestral"
    combi(orc, "Full Orchestra", [T("Full Strings"), T("French Horns", level=0.5), T("Flute", level=0.4, transpose=12), T("Tuba", level=0.4, keyHi=55)])
    combi(orc, "Epic Strings", [T("Marcato Strings"), T("Trombone", level=0.5), T("Cinematic Pad", level=0.35)])
    combi(orc, "Woodwind Choir", [T("Flute"), T("Clarinet", level=0.6), T("Oboe", level=0.5)])
    combi(orc, "Harp & Strings", [T("Concert Harp"), T("Slow Strings", level=0.45)])
    combi(orc, "Orchestra Hit Layer", [T("Orchestra Hit"), T("Orchestral Stab", level=0.6)])
    combi(orc, "Celesta Orchestra", [T("Celesta"), T("Chamber Strings", level=0.5), T("Flute", level=0.35)])
    combi(orc, "Choir & Orchestra", [T("Choir Aahs"), T("Full Strings", level=0.5), T("French Horns", level=0.4)])
    combi(orc, "Pizz Ensemble", [T("Pizzicato"), T("Marimba", level=0.4)])

    w = "World"
    combi(w, "Koto Garden", [T("Koto"), T("Shakuhachi", level=0.5)])
    combi(w, "Sitar Drone", [T("Sitar"), T("Bagpipe Drone", level=0.35, keyHi=54)])
    combi(w, "Pan Flute & Harp", [T("Pan Flute"), T("Concert Harp", level=0.5)])
    combi(w, "Island Steel", [T("Steel Drum"), T("Kalimba", level=0.5)])
    combi(w, "Bayou", [T("Banjo"), T("Harmonica", level=0.6)])
    combi(w, "Oud & Strings", [T("Oud"), T("Dark Strings", level=0.4)])
    combi(w, "Gamelan Night", [T("Gamelan"), T("Bowl Chime", level=0.5), T("Space Drone", level=0.25)])
    combi(w, "Paris Café", [T("Accordion"), T("Upright Bass", **lo)])

    gt = "Guitar"
    combi(gt, "Acoustic Duo", [T("Nylon Guitar"), T("Steel Guitar", level=0.6)])
    combi(gt, "12-String & Pad", [T("12-String"), T("Warm Pad", level=0.35)])
    combi(gt, "Bass / Clean Guitar", [T("Finger Bass", **lo), T("Clean Electric", **hi)])
    combi(gt, "Rock Section", [T("Pick Bass", **lo), T("Power Chords", **hi)])
    combi(gt, "Ambient Strums", [T("Ambient Guitar"), T("Glass Pad", level=0.35)])
    combi(gt, "Funk Section", [T("Slap Bass", **lo), T("Funk Guitar", **hi)])
    combi(gt, "Harp & Guitar", [T("Concert Harp"), T("Nylon Guitar", level=0.6)])
    combi(gt, "Mandolin & Dulcimer", [T("Mandolin"), T("Dulcimer", level=0.6)])

    bs = "Bass Splits"
    combi(bs, "Synth Bass / Lead", [T("Classic Synth Bass", **lo), T("Saw Lead", **hi)])
    combi(bs, "Acid / Pad", [T("Acid Bass", **lo), T("Sweep Pad", **hi)])
    combi(bs, "Reese / Pluck", [T("Reese Bass", **lo), T("Trance Pluck", **hi)])
    combi(bs, "Sub / Super Saw", [T("Sub Bass", **lo), T("Super Saw", **hi)])
    combi(bs, "FM Bass / Keys", [T("FM Bass", **lo), T("Digital Keys", **hi)])
    combi(bs, "Fretless / EP", [T("Fretless Bass", **lo), T("Mellow Tine", **hi)])
    combi(bs, "Upright / Vibes", [T("Upright Bass", **lo), T("Vibraphone", **hi)])
    combi(bs, "Deep House Split", [T("Deep House Bass", **lo), T("Chord Stab", **hi)])

    sy = "Synth"
    combi(sy, "Poly Stack", [T("Poly Synth"), T("80s Poly", level=0.6)])
    combi(sy, "Super Stack", [T("Super Saw"), T("Synth Pluck", level=0.6)])
    combi(sy, "Juno & Strings", [T("Juno Keys"), T("String Machine", level=0.5)])
    combi(sy, "Glass & Bell", [T("Glass Keys"), T("Bell Pluck", level=0.6)])
    combi(sy, "Brass Keys Layer", [T("Brass Keys"), T("Poly Brass", level=0.5)])
    combi(sy, "Twin Stack", [T("Twin Stab"), T("Square Keys", level=0.5)])
    combi(sy, "Hoover Rave", [T("Hoover"), T("Rave Stab", level=0.5)])
    combi(sy, "Digital Dream", [T("Digital Keys"), T("Dream Pad", level=0.4)])

    ld = "Lead"
    combi(ld, "Dual Lead", [T("Saw Lead"), T("Square Lead", level=0.6)])
    combi(ld, "Lead & Pad", [T("Sync Lead", **hi), T("Warm Pad", **lo)])
    combi(ld, "Fat Fifths", [T("Fat Lead"), T("Fifth Lead", level=0.5)])
    combi(ld, "Prog Rock", [T("Prog Lead", **hi), T("Rock Organ", **lo)])
    combi(ld, "Theremin Space", [T("Theremin"), T("Space Pad", level=0.4)])
    combi(ld, "Chip Duo", [T("Chip Lead"), T("Chip Lead", transpose=12, level=0.5)])
    combi(ld, "Screaming Layer", [T("Scream Lead"), T("Mini Lead", level=0.5)])
    combi(ld, "Bass & Mini Lead", [T("Classic Synth Bass", **lo), T("Mini Lead", **hi)])

    mo = "Motion"
    combi(mo, "Vector Motion", [T("Filter Wobble", vector=True), T("Phase Sweep", vector=True), T("Pulsing Pad", vector=True), T("Vowel Morph", vector=True)])
    combi(mo, "Bubbling Pads", [T("S&H Bubbles"), T("Warm Pad", level=0.4)])
    combi(mo, "Gated Choir", [T("Gated Pad"), T("Choir Aahs", level=0.4)])
    combi(mo, "Flange & Phase", [T("Flanged Pad"), T("Phase Sweep", level=0.5)])
    combi(mo, "Wah Funk", [T("Wah Motion"), T("Funk Guitar", level=0.5)])
    combi(mo, "Resonant Worlds", [T("Resonant Sweep"), T("Evolving Wave", level=0.5)])
    combi(mo, "Rhythmic Layers", [T("Rhythmic Wave"), T("Pulse Width Dance", level=0.5)])
    combi(mo, "Vector Synths", [T("Poly Synth", vector=True), T("Glass Keys", vector=True), T("Juno Keys", vector=True), T("Digital Keys", vector=True)])

    se = "SE/Hits"
    combi(se, "Storm", [T("Thunder", **lo), T("Wind", **hi), T("Rain", level=0.4)])
    combi(se, "Sci-Fi Lab", [T("UFO"), T("Bubbles", level=0.5)])
    combi(se, "Alarm Zone", [T("Alarm"), T("Siren", level=0.5)])
    combi(se, "Big Hit", [T("Orchestra Hit"), T("Synth Hit", level=0.6), T("Brass Hit", level=0.5)])
    combi(se, "Riser & Drop", [T("Riser", **hi), T("Downer", **lo)])
    combi(se, "Drone Field", [T("Space Drone"), T("Dark Drone", level=0.6)])
    combi(se, "Glitch Machine", [T("Glitch"), T("Radio Noise", level=0.5)])
    combi(se, "Seaside", [T("Ocean Waves"), T("Wind", level=0.5)])

    ar = "Arpeggio"
    A = {"arpOn": True, "arpMode": "Up", "arpRate": "1/16", "arpOct": 2, "arpGate": 0.45, "dlySync": "1/8D"}
    combi(ar, "Arp & Pad", [T("Pluck Arp", **hi), T("Warm Pad", **lo)], A)
    combi(ar, "Bell Cascade", [T("Arp Bells"), T("Glass Pad", level=0.4)], dict(A, arpMode="Up-Down", arpOct=3))
    combi(ar, "Trance Engine", [T("Trance Gate"), T("Sub Bass", **lo)], dict(A, arpMode="Chord"))
    combi(ar, "Random Stars", [T("Random Blips"), T("Space Pad", level=0.4)], dict(A, arpMode="Random", arpOct=3))
    combi(ar, "Harp Glissando", [T("Harp Arp"), T("Slow Strings", level=0.4)], dict(A, arpRate="1/16T", arpOct=3, arpGate=0.9))
    combi(ar, "Sequencer", [T("Sequence Pluck"), T("Arp Bass", level=0.6)], dict(A, arpMode="Up-Down"))
    combi(ar, "Chord Machine", [T("Chord Arp"), T("Juno Keys", level=0.4)], dict(A, arpMode="Chord", arpRate="1/8"))
    combi(ar, "Twin Groove", [T("Twin Seq"), T("Deep House Bass", level=0.5)], dict(A, arpRate="1/16", arpOct=1, arpSwing=0.2))

    dr = "Drums/Splits"
    dl = dict(keyHi=59)
    combi(dr, "Drums / Bass", [T("Standard Kit", **dl), T("Finger Bass", keyLo=60, transpose=-24)])
    combi(dr, "Electro / Lead", [T("Electro Kit", **dl), T("Saw Lead", keyLo=60)])
    combi(dr, "House / Stabs", [T("House Kit", **dl), T("Chord Stab", keyLo=60)])
    combi(dr, "Rock / Organ", [T("Rock Kit", **dl), T("Rock Organ", keyLo=60)])
    combi(dr, "Jazz Trio", [T("Jazz Kit", **dl), T("Upright Bass", keyLo=60, keyHi=71, transpose=-24), T("Jazz Grand", keyLo=72, transpose=-12)])
    combi(dr, "Trap / Bells", [T("Trap Kit", **dl), T("Bell Pluck", keyLo=60)])
    combi(dr, "Techno / Acid", [T("Techno Kit", **dl), T("Acid Bass", keyLo=60, transpose=-24)])
    combi(dr, "Hip-Hop / Keys", [T("Hip-Hop Kit", **dl), T("Lo-Fi Keys", keyLo=60)])

    # fill each combi category to 8 (some above have more)
    return C


# ---------------------------------------------------------------------------------------------
def validate(programs, combis):
    for pr in programs:
        for k in pr["p"]:
            assert k in TIMBRE_IDS, (pr["name"], k)
        for k in pr["g"]:
            assert k in GLOBAL_IDS, (pr["name"], k)
    for c in combis:
        assert c["cat"] in COMBI_CATS, c["name"]
        for t in c["t"]:
            if t:
                for k in t["p"]:
                    assert k in TIMBRE_IDS, (c["name"], k)
        for k in c["g"]:
            assert k in GLOBAL_IDS, (c["name"], k)


def clean(v):
    if isinstance(v, float):
        return round(v, 4)
    return v


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--levels", default=os.path.join(ROOT, "tools", "measured_levels.csv"),
                    help="CSV from RC2Tests --loudness, measured on the un-balanced bank (see README)")
    ap.add_argument("--no-levels", action="store_true", help="write recipe levels only (to re-measure)")
    args = ap.parse_args()
    if args.no_levels:
        args.levels = None

    programs = build_programs()
    names = {p["name"] for p in programs}
    combis = build_combis(names)
    validate(programs, combis)

    if args.levels:
        target = 0.12  # held-chord RMS target
        meas = {}
        with open(args.levels) as f:
            for row in csv.reader(f):
                if len(row) >= 3 and row[0] == "P" and os.path.exists(args.levels):
                    meas[row[1]] = float(row[2])
        adjusted = 0
        for pr in programs:
            rms = meas.get(pr["name"])
            if not rms or rms <= 0:
                continue
            cur = pr["p"].get("level", 0.75)
            gain = target / rms
            gain = max(0.5, min(2.5, gain))
            pr["p"]["level"] = round(max(0.15, min(1.0, cur * gain)), 3)
            adjusted += 1
        print("auto-balanced %d program levels" % adjusted)

    for pr in programs:
        pr["p"] = {k: clean(v) for k, v in pr["p"].items()}
        pr["g"] = {k: clean(v) for k, v in pr["g"].items()}
        if not pr["g"]:
            del pr["g"]

    data = {"version": 1, "programs": programs, "combis": combis}
    with open(OUT, "w") as f:
        json.dump(data, f, indent=None, separators=(",", ":"), ensure_ascii=False)
        f.write("\n")
    per = {}
    for pr in programs:
        per[pr["cat"]] = per.get(pr["cat"], 0) + 1
    print("wrote %s: %d programs, %d combis" % (OUT, len(programs), len(combis)))
    for c in PROGRAM_CATS:
        print("  %-15s %d" % (c, per.get(c, 0)))


if __name__ == "__main__":
    main()
