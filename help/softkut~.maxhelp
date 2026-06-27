{
    "patcher": {
        "fileversion": 1,
        "appversion": {
            "major": 9,
            "minor": 1,
            "revision": 4,
            "architecture": "x64",
            "modernui": 1
        },
        "classnamespace": "box",
        "rect": [ 173.0, 159.0, 640.0, 620.0 ],
        "boxes": [
            {
                "box": {
                    "fontname": "Arial",
                    "fontsize": 13.0,
                    "id": "obj-8",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 458.0, 100.0, 119.0, 23.0 ],
                    "text": "read vibes-a1.aif"
                }
            },
            {
                "box": {
                    "fontsize": 14.0,
                    "id": "obj-1",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 20.0, 14.0, 480.0, 22.0 ],
                    "text": "softkut~ : softcut-lib looper, 6 voices sharing a mono buffer~"
                }
            },
            {
                "box": {
                    "id": "obj-2",
                    "linecount": 2,
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 20.0, 38.0, 600.0, 33.0 ],
                    "text": "Per-voice control messages take <voice> <value>. Record a tone into voice 0, then loop-play it. Voice outlets 0-5 are dry; outlets 6/7 are the panned stereo mix; outlet 8 reports phase/position."
                }
            },
            {
                "box": {
                    "id": "obj-buf",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "float", "bang" ],
                    "patching_rect": [ 458.0, 145.0, 108.0, 22.0 ],
                    "text": "buffer~ skbuf 4096"
                }
            },
            {
                "box": {
                    "id": "obj-tone",
                    "maxclass": "newobj",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "signal" ],
                    "patching_rect": [ 20.0, 110.0, 80.0, 22.0 ],
                    "text": "cycle~ 220"
                }
            },
            {
                "box": {
                    "id": "obj-set",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 150.0, 110.0, 80.0, 22.0 ],
                    "text": "set skbuf"
                }
            },
            {
                "box": {
                    "id": "obj-rec1",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 150.0, 145.0, 62.0, 22.0 ],
                    "text": "rec 0 1"
                }
            },
            {
                "box": {
                    "id": "obj-rec0",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 220.0, 145.0, 62.0, 22.0 ],
                    "text": "rec 0 0"
                }
            },
            {
                "box": {
                    "id": "obj-play1",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 150.0, 178.0, 62.0, 22.0 ],
                    "text": "play 0 1"
                }
            },
            {
                "box": {
                    "id": "obj-play0",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 220.0, 178.0, 62.0, 22.0 ],
                    "text": "play 0 0"
                }
            },
            {
                "box": {
                    "id": "obj-loop1",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 300.0, 145.0, 62.0, 22.0 ],
                    "text": "loop 0 1"
                }
            },
            {
                "box": {
                    "id": "obj-rate1",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 300.0, 178.0, 62.0, 22.0 ],
                    "text": "rate 0 1"
                }
            },
            {
                "box": {
                    "id": "obj-rate05",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 370.0, 178.0, 76.0, 22.0 ],
                    "text": "rate 0 0.5"
                }
            },
            {
                "box": {
                    "id": "obj-lev1",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 150.0, 211.0, 70.0, 22.0 ],
                    "text": "level 0 1."
                }
            },
            {
                "box": {
                    "id": "obj-lev05",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 228.0, 211.0, 82.0, 22.0 ],
                    "text": "level 0 0.5"
                }
            },
            {
                "box": {
                    "id": "obj-panL",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 320.0, 211.0, 64.0, 22.0 ],
                    "text": "pan 0 -1"
                }
            },
            {
                "box": {
                    "id": "obj-panR",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 392.0, 211.0, 60.0, 22.0 ],
                    "text": "pan 0 1"
                }
            },
            {
                "box": {
                    "id": "obj-pos",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 150.0, 244.0, 90.0, 22.0 ],
                    "text": "position 0 0."
                }
            },
            {
                "box": {
                    "id": "obj-poll",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 250.0, 244.0, 50.0, 22.0 ],
                    "text": "poll"
                }
            },
            {
                "box": {
                    "id": "obj-sk",
                    "maxclass": "newobj",
                    "numinlets": 6,
                    "numoutlets": 9,
                    "outlettype": [ "", "signal", "signal", "signal", "signal", "signal", "signal", "signal", "signal" ],
                    "patching_rect": [ 20.0, 300.0, 320.0, 22.0 ],
                    "text": "softkut~ skbuf @report 100"
                }
            },
            {
                "box": {
                    "id": "obj-dac",
                    "maxclass": "ezdac~",
                    "numinlets": 2,
                    "numoutlets": 0,
                    "patching_rect": [ 245.75, 355.0, 45.0, 45.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-scope",
                    "maxclass": "scope~",
                    "numinlets": 2,
                    "numoutlets": 0,
                    "patching_rect": [ 321.0, 420.0, 200.0, 130.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-print",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 25.0, 420.0, 90.0, 22.0 ],
                    "text": "print report"
                }
            }
        ],
        "lines": [
            {
                "patchline": {
                    "destination": [ "obj-buf", 0 ],
                    "source": [ "obj-8", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-sk", 0 ],
                    "source": [ "obj-lev05", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-sk", 0 ],
                    "source": [ "obj-lev1", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-sk", 0 ],
                    "source": [ "obj-loop1", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-sk", 0 ],
                    "source": [ "obj-panL", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-sk", 0 ],
                    "source": [ "obj-panR", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-sk", 0 ],
                    "source": [ "obj-play0", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-sk", 0 ],
                    "source": [ "obj-play1", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-sk", 0 ],
                    "source": [ "obj-poll", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-sk", 0 ],
                    "source": [ "obj-pos", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-sk", 0 ],
                    "source": [ "obj-rate05", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-sk", 0 ],
                    "source": [ "obj-rate1", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-sk", 0 ],
                    "source": [ "obj-rec0", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-sk", 0 ],
                    "source": [ "obj-rec1", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-sk", 0 ],
                    "source": [ "obj-set", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-dac", 1 ],
                    "source": [ "obj-sk", 7 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-dac", 0 ],
                    "source": [ "obj-sk", 6 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-scope", 0 ],
                    "source": [ "obj-sk", 8 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-sk", 0 ],
                    "source": [ "obj-tone", 0 ]
                }
            }
        ],
        "autosave": 0
    }
}