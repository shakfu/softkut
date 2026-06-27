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
        "rect": [ 165.0, 246.0, 640.0, 620.0 ],
        "boxes": [
            {
                "box": {
                    "id": "obj-6",
                    "maxclass": "scope~",
                    "numinlets": 2,
                    "numoutlets": 0,
                    "patching_rect": [ 171.0, 443.0, 130.0, 130.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-5",
                    "maxclass": "mc.ezdac~",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 18.0, 443.0, 45.0, 45.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-3",
                    "maxclass": "newobj",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "multichannelsignal" ],
                    "patching_rect": [ 18.0, 392.0, 68.0, 22.0 ],
                    "text": "mc.stereo~"
                }
            },
            {
                "box": {
                    "id": "obj-25",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 532.0, 357.0, 35.0, 22.0 ],
                    "text": "clear"
                }
            },
            {
                "box": {
                    "id": "obj-23",
                    "linecount": 5,
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 257.0, 84.0, 127.0, 76.0 ],
                    "text": "prelevel 0 0.75, reclevel 0 1, recpreslew 0 0.05, loop 0 1, rec 0 1, play 0 1"
                }
            },
            {
                "box": {
                    "id": "obj-21",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 514.0, 254.0, 97.0, 22.0 ],
                    "text": "recpreslew 0 0.5"
                }
            },
            {
                "box": {
                    "id": "obj-19",
                    "maxclass": "newobj",
                    "numinlets": 6,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 505.0, 171.0, 97.0, 22.0 ],
                    "text": "scale 0 127 0. 1."
                }
            },
            {
                "box": {
                    "id": "obj-18",
                    "maxclass": "newobj",
                    "numinlets": 6,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 397.0, 171.0, 97.0, 22.0 ],
                    "text": "scale 0 127 0. 1."
                }
            },
            {
                "box": {
                    "id": "obj-17",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 505.0, 202.0, 77.0, 22.0 ],
                    "text": "reclevel 0 $1"
                }
            },
            {
                "box": {
                    "id": "obj-11",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 400.5, 202.0, 77.0, 22.0 ],
                    "text": "prelevel 0 $1"
                }
            },
            {
                "box": {
                    "id": "obj-9",
                    "maxclass": "pictslider",
                    "numinlets": 2,
                    "numoutlets": 2,
                    "outlettype": [ "int", "int" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 397.0, 77.0, 82.0, 83.0 ]
                }
            },
            {
                "box": {
                    "data": {
                        "clips": [
                            {
                                "absolutepath": "cello-f2.aif",
                                "filename": "cello-f2.aif",
                                "filekind": "audiofile",
                                "id": "u287003680",
                                "loop": 0,
                                "content_state": {
                                    "loop": 0
                                }
                            }
                        ]
                    },
                    "id": "obj-4",
                    "maxclass": "playlist~",
                    "mode": "basic",
                    "numinlets": 1,
                    "numoutlets": 5,
                    "outlettype": [ "signal", "signal", "signal", "", "dictionary" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 18.0, 171.0, 150.0, 30.0 ],
                    "quality": "basic",
                    "saved_attribute_attributes": {
                        "candicane2": {
                            "expression": ""
                        },
                        "candicane3": {
                            "expression": ""
                        },
                        "candicane4": {
                            "expression": ""
                        },
                        "candicane5": {
                            "expression": ""
                        },
                        "candicane6": {
                            "expression": ""
                        },
                        "candicane7": {
                            "expression": ""
                        },
                        "candicane8": {
                            "expression": ""
                        }
                    }
                }
            },
            {
                "box": {
                    "fontname": "Arial",
                    "fontsize": 13.0,
                    "id": "obj-8",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 395.0, 356.5, 119.0, 23.0 ],
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
                    "text": "mc.softkut~ : softcut-lib looper, 6 voices sharing a mono buffer~"
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
                    "patching_rect": [ 395.0, 401.5, 108.0, 22.0 ],
                    "text": "buffer~ skbuf 4096"
                }
            },
            {
                "box": {
                    "id": "obj-set",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 171.0, 171.0, 80.0, 22.0 ],
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
                    "patching_rect": [ 171.0, 206.0, 62.0, 22.0 ],
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
                    "patching_rect": [ 241.0, 206.0, 62.0, 22.0 ],
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
                    "patching_rect": [ 171.0, 239.0, 62.0, 22.0 ],
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
                    "patching_rect": [ 241.0, 239.0, 62.0, 22.0 ],
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
                    "patching_rect": [ 321.0, 206.0, 62.0, 22.0 ],
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
                    "patching_rect": [ 321.0, 239.0, 62.0, 22.0 ],
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
                    "patching_rect": [ 391.0, 239.0, 76.0, 22.0 ],
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
                    "patching_rect": [ 171.0, 272.0, 70.0, 22.0 ],
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
                    "patching_rect": [ 249.0, 272.0, 82.0, 22.0 ],
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
                    "patching_rect": [ 341.0, 272.0, 64.0, 22.0 ],
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
                    "patching_rect": [ 413.0, 272.0, 60.0, 22.0 ],
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
                    "patching_rect": [ 171.0, 305.0, 90.0, 22.0 ],
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
                    "patching_rect": [ 271.0, 305.0, 50.0, 22.0 ],
                    "text": "poll"
                }
            },
            {
                "box": {
                    "id": "obj-sk",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "multichannelsignal", "" ],
                    "patching_rect": [ 18.0, 357.0, 172.0, 22.0 ],
                    "text": "mc.softkut~ skbuf @report 100"
                }
            },
            {
                "box": {
                    "id": "obj-print",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 171.0, 396.0, 90.0, 22.0 ],
                    "text": "print report"
                }
            }
        ],
        "lines": [
            {
                "patchline": {
                    "destination": [ "obj-sk", 0 ],
                    "source": [ "obj-11", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-sk", 0 ],
                    "source": [ "obj-17", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-11", 0 ],
                    "source": [ "obj-18", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-17", 0 ],
                    "source": [ "obj-19", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-sk", 0 ],
                    "source": [ "obj-21", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-sk", 0 ],
                    "source": [ "obj-23", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-buf", 0 ],
                    "source": [ "obj-25", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-5", 0 ],
                    "order": 1,
                    "source": [ "obj-3", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-6", 0 ],
                    "order": 0,
                    "source": [ "obj-3", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-sk", 0 ],
                    "source": [ "obj-4", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-buf", 0 ],
                    "source": [ "obj-8", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-18", 0 ],
                    "source": [ "obj-9", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-19", 0 ],
                    "source": [ "obj-9", 1 ]
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
                    "destination": [ "obj-3", 0 ],
                    "source": [ "obj-sk", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-print", 0 ],
                    "source": [ "obj-sk", 1 ]
                }
            }
        ],
        "autosave": 0
    }
}