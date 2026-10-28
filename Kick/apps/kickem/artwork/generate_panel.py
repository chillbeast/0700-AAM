"""Editable vector reconstruction of kick-flat.png; no raster images are embedded.

The transparent hit-regions group is also the simulator's layout source.
Run this script from any directory after changing the geometry below.
"""
from pathlib import Path
from html import escape
import math
import xml.etree.ElementTree as ET

here = Path(__file__).resolve().parent
parts = ['''<svg xmlns="http://www.w3.org/2000/svg" width="1360" height="1350" viewBox="0 0 680 675">
<title>KICK hardware panel</title>
<desc>Vector panel reconstructed from the supplied kick-flat.png. Transparent named rectangles define interactive control bounds.</desc>
<defs>
 <linearGradient id="body" x2="0" y2="1"><stop stop-color="#353639"/><stop offset=".025" stop-color="#242527"/><stop offset="1" stop-color="#202123"/></linearGradient>
 <linearGradient id="bevel" x2=".8" y2="1"><stop stop-color="#737478"/><stop offset=".16" stop-color="#36373a"/><stop offset=".78" stop-color="#0c0d0e"/><stop offset="1" stop-color="#4e4f52"/></linearGradient>
 <linearGradient id="pad" x2="1" y2="1"><stop stop-color="#414245"/><stop offset=".44" stop-color="#2a2b2e"/><stop offset="1" stop-color="#141517"/></linearGradient>
 <linearGradient id="key" x2="0" y2="1"><stop stop-color="#343538"/><stop offset=".25" stop-color="#27282b"/><stop offset="1" stop-color="#191a1c"/></linearGradient>
 <radialGradient id="knob"><stop stop-color="#595b60"/><stop offset=".25" stop-color="#26272a"/><stop offset=".7" stop-color="#3b3d41"/><stop offset=".9" stop-color="#101113"/><stop offset="1" stop-color="#4d4f53"/></radialGradient>
 <radialGradient id="wheel"><stop stop-color="#54565b"/><stop offset=".45" stop-color="#202124"/><stop offset=".72" stop-color="#383a3e"/><stop offset=".91" stop-color="#090a0b"/><stop offset="1" stop-color="#626469"/></radialGradient>
</defs>
<rect x=".75" y=".75" width="678.5" height="673.5" rx="9" fill="url(#bevel)"/>
<rect x="7" y="6" width="666" height="662" rx="5" fill="url(#body)"/>
<path d="M18 2H581M625 2H661" stroke="#8c8d90" stroke-width="1.2" fill="none"/>
<rect x="113" y="23" width="454" height="282" rx="2" fill="#111214" stroke="#111"/>
<path d="M115 24H565V303" stroke="#4d4e51" stroke-width="1" fill="none"/>
<rect x="118" y="52" width="430" height="251.953125" fill="#000"/>
''']
hits = []


def rect(x, y, w, h, fill, stroke="none", radius=0, extra=""):
    parts.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{radius}" fill="{fill}" stroke="{stroke}" {extra}/>')


def text(value, x, y, size=7.5, color="#d4d4d6", weight="bold", anchor="middle"):
    parts.append(f'<text x="{x}" y="{y}" font-family="Arial, sans-serif" font-size="{size}" font-weight="{weight}" text-anchor="{anchor}" fill="{color}">{escape(value)}</text>')


def hit(name, x, y, w, h):
    hits.append((name, x, y, w, h))


def key(name, x, y, w, h, label, alternate="", color="#c8c9cc"):
    rect(x-1.3, y-1.2, w+2.6, h+2.5, "#0b0c0d", "#2f3033", 1.2)
    rect(x, y, w, h, "url(#key)", "#414246", .8)
    for i, line in enumerate(label.split("\n")):
        text(line, x+w/2, y+h/2+(i-(len(label.split("\n"))-1)/2)*7+2.5, 6.4, color)
    if alternate:
        text(alternate, x+w/2, y+h+10, 6.5, "#b8b9bd")
    hit(name, x, y, w, h)


def knob(name, cx, cy, r=17):
    parts.append(f'<circle cx="{cx}" cy="{cy+1.2}" r="{r+1.8}" fill="#111214"/>')
    parts.append(f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="url(#knob)" stroke="#08090a"/>')
    for i in range(12):
        a = i*math.tau/12
        x1,y1 = cx+math.sin(a)*r*.55,cy+math.cos(a)*r*.55
        x2,y2 = cx+math.sin(a)*r*.87,cy+math.cos(a)*r*.87
        parts.append(f'<path d="M{x1:.2f} {y1:.2f}L{x2:.2f} {y2:.2f}" stroke="#85868b" stroke-opacity=".55" stroke-width="1.5"/>')
    hit(name, cx-r-1, cy-r-1, 2*r+2, 2*r+2)


# Reuse the repository's existing, outlined Behringer wordmark.
logo = ET.parse(here.parents[2]/"assets/Default/Logo_Splash-Page.svg").getroot()
parts.append('<g transform="translate(294 25) scale(.48)" opacity=".78">')
for child in logo:
    for node in child.iter():
        node.tag = node.tag.split("}")[-1]
    parts.append(ET.tostring(child, encoding="unicode"))
parts.append('</g>')
rect(25, 25, 70, 31, "url(#bevel)", "#151618", 2)
text("PRO", 60, 46, 16, "#a5a6a8")
text("MASTER", 60, 89, 8)
knob("master", 60, 114, 17)
text("MIN", 43, 142, 6)
text("MAX", 78, 142, 6)
text("REC", 60, 168, 8)
knob("rec-gain", 60, 193, 17)
text("MIN", 43, 220, 6)
text("MAX", 78, 220, 6)
for i, cy in enumerate([65,130,194,259],1):
    knob(f"p{i}",621,cy,17)
    text(f"P{i}",621,cy+34,6.5)
rect(25, 245, 71, 57, "url(#bevel)", "#161719", 2)
for x in [35,51,67,83]:
    rect(x-2.5, 251, 5, 3, "#ad4151", radius=.6)
key("link",42,268,36,23,"LINK",color="#56b6df")

# Standalone SVG displays a vector boot graphic; the running UI replaces this aperture.
parts.append('<g fill="#e6e6e8" transform="translate(254 149)">')
parts.append('<path d="M0 0H12V31H0ZM15 15L37 0H58L35 15L60 31H38ZM65 0H78V31H65ZM86 0H126V8H99V23H126V31H86ZM134 0H146V31H134ZM149 15L171 0H192L169 15L194 31H172Z"/>')
parts.append('</g>')
text("KICK", 333, 296, 18, "#e4e4e6")
hit("screen",118,52,430,251.953125)

# Touch strip and sample pads.
parts.append('<path d="M17 343H76L71 350H22V480L76 487H17Z" fill="url(#bevel)"/>')
rect(23,350,47,129,"#292a2d")
for i in range(12):
    rect(60,356+i*10,3.6,5.5,"#4a4b4f",radius=.6)
hit("slider",19,345,55,139)
key("full-level",29,518,36,22,"FULL\nLEVEL","HALF LVL")
key("16-levels",29,577,36,23,"16\nLEVELS","NOTES")
key("erase",29,634,36,21,"ERASE")
hit("pads",99,341,310,315)
for i in range(16):
    x, y = 101+(i%4)*80, 343+(3-i//4)*80
    rect(x-2,y-2,68,68,"#111214","#1b1c1f",1.5)
    rect(x,y,65,65,"url(#pad)","#5b5c60",1.4)
    parts.append(f'<path d="M{x+2} {y+1}H{x+62}" stroke="#75767a" stroke-opacity=".6"/>')
    text(str(i+1),x+2,y+74,6.4,"#c8c9cc",anchor="start")
    hit(f"pad-{i}",x-1,y-1,67,67)

parts.append('<path d="M433 334H531M568 334H663" stroke="#96979b" stroke-width="1"/>')
text("BANK",548,337,8)
for name,x,color,alternate in zip(["bank-a","bank-b","bank-c","bank-d"],[435,499,563,627],["#df424c","#d1ac45","#67b4e1","#86b946"],"EFGH"):
    key(name,x,345,33,21,name[-1].upper(),alternate,color)
for name,x,label,alt in [
    ("main",435,"MAIN","GRID"),("browse",483,"BROWSE","SAVE"),
    ("track-mix",531,"TRACK\nMIX","PAD MIX"),("track-mute",579,"TRACK\nMUTE","PAD MUTE"),("next-seq",627,"NEXT\nSEQ","FX")
]:
    key(name,x,402,34,22,label,alt)
key("tap",435,460,34,22,"TAP\nTEMPO","MASTER")
key("repeat",435,518,34,22,"NOTE\nREPEAT","LATCH")
key("menu",627,460,34,22,"MENU","PREFS")
key("shift",627,512,34,32,"SHIFT")
parts.append('<path d="M627 550H662" stroke="#bbbcc0" stroke-width="1.1"/>')

# Four navigation sectors around the jog wheel, with a centre push/encoder.
rect(492,448,112,110,"url(#bevel)","#0c0d0e",5)
for name,x,y in [("back",493,449),("copy",549,449),("minus",493,504),("plus",549,504)]:
    rect(x,y,54,52,"url(#key)","#0d0e10",2)
    hit(name,x,y,54,52)
parts.append('<path d="M500 458h7a5 5 0 0 1 0 10h-7m0-10 3-3m-3 3 3 3" fill="none" stroke="#a4a5aa" stroke-width="1.1"/>')
parts.append('<path d="M588 454h8v9h-8Zm3 3h8v9h-8Z" fill="none" stroke="#a4a5aa" stroke-width="1"/>')
text("−",502,544,11,"#bdc0c5")
text("+",595,544,11,"#bdc0c5")
parts.append('<circle cx="548" cy="503" r="43" fill="url(#wheel)" stroke="#0a0b0c" stroke-width="1.5"/>')
parts.append('<circle cx="548" cy="503" r="29.5" fill="#111214" stroke="#8a243c" stroke-width="4"/>')
parts.append('<circle cx="548" cy="503" r="27.2" fill="url(#knob)" stroke="#d25567" stroke-width="1.1"/>')
for i in range(8):
    a=i*math.tau/8
    parts.append(f'<path d="M548 503L{548+math.sin(a)*24:.2f} {503+math.cos(a)*24:.2f}" stroke="#84858a" stroke-opacity=".45" stroke-width="1.6"/>')
hit("jog",505,460,86,86)
for name,x,label,alt in [
    ("step",435,"STEP\nSEQ","AUTO"),("tc",483,"TC","ON/OFF"),("sampler",531,"SAMPLER","LOOP"),
    ("sample-edit",579,"SAMPLE\nEDIT","LINK EDIT"),("program-edit",627,"PROG\nEDIT","MIDI CNTRL")
]:
    key(name,x,575,34,22,label,alt)
for name,cx,color in [("record",452,"#ed313f"),("overdub",500,"#c2c3c7"),("stop",548,"#e2b93c"),("play",596,"#78ba49"),("play-start",644,"#51acd0")]:
    parts.append(f'<circle cx="{cx}" cy="643" r="17" fill="url(#knob)" stroke="#101113" stroke-width="1.6"/>')
    if name=="record":
        parts.append(f'<circle cx="{cx}" cy="643" r="5" fill="{color}"/>')
    elif name=="stop":
        rect(cx-5,638,10,10,color,radius=.6)
    elif name in ["play","play-start"]:
        if name=="play-start": rect(cx-7,638,2,10,color)
        parts.append(f'<path d="M{cx-3} 637l10 6-10 6Z" fill="{color}"/>')
    else:
        parts.append(f'<circle cx="{cx}" cy="643" r="5.5" fill="none" stroke="{color}" stroke-width="1.2"/><path d="M{cx-3} 643h6m-3-3v6" stroke="{color}"/>')
    hit(name,cx-18,625,36,36)
parts.append('<g id="hit-regions" opacity="0" fill="#000">')
for name,x,y,w,h in hits:
    parts.append(f'<rect id="{name}" x="{x}" y="{y}" width="{w}" height="{h}"/>')
parts.append('</g></svg>')
(here/"kick-panel.svg").write_text("\n".join(parts)+"\n",encoding="utf-8")
print(f"Wrote vector panel with {len(hits)} named regions.")
