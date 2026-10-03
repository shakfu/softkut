{
    "patcher": {
        "fileversion": 1,
        "appversion": {
            "major": 8,
            "minor": 5,
            "revision": 5,
            "architecture": "x64",
            "modernui": 1
        },
        "classnamespace": "box",
        "rect": [
            100.0,
            100.0,
            560.0,
            560.0
        ],
        "bglocked": 0,
        "openinpresentation": 0,
        "default_fontsize": 12.0,
        "default_fontface": 0,
        "default_fontname": "Arial",
        "gridonopen": 1,
        "gridsize": [
            15.0,
            15.0
        ],
        "gridsnaponopen": 1,
        "objectsnaponopen": 1,
        "statusbarvisible": 2,
        "toolbarvisible": 1,
        "lefttoolbarpinned": 0,
        "toptoolbarpinned": 0,
        "righttoolbarpinned": 0,
        "bottomtoolbarpinned": 0,
        "toolbars_unpinned_last_save": 0,
        "tallnewobj": 0,
        "boxanimatetime": 200,
        "enablehscroll": 1,
        "enablevscroll": 1,
        "devicewidth": 0.0,
        "description": "",
        "digest": "",
        "tags": "",
        "style": "",
        "subpatcher_template": "",
        "assistshowspatchername": 0,
        "boxes": [
            {
                "box": {
                    "id": "obj-1",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "patching_rect": [
                        0.0,
                        0.0,
                        150.0,
                        22.0
                    ],
                    "text": "voice / channel",
                    "outlettype": [
                        ""
                    ]
                }
            },
            {
                "box": {
                    "id": "obj-2",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "patching_rect": [
                        158.0,
                        0.0,
                        90.0,
                        22.0
                    ],
                    "text": "stopped",
                    "outlettype": [
                        ""
                    ]
                }
            },
            {
                "box": {
                    "id": "obj-3",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [
                        256.0,
                        0.0,
                        260.0,
                        36.0
                    ],
                    "text": "play head = moving cursor; loop window = strip below"
                }
            },
            {
                "box": {
                    "id": "obj-4",
                    "maxclass": "waveform~",
                    "numinlets": 5,
                    "numoutlets": 6,
                    "patching_rect": [
                        0.0,
                        40.0,
                        520.0,
                        112.0
                    ],
                    "outlettype": [
                        "float",
                        "float",
                        "float",
                        "float",
                        "list",
                        ""
                    ],
                    "parameter_enable": 0,
                    "setmode": 1,
                    "allowdrag": 0
                }
            },
            {
                "box": {
                    "id": "obj-5",
                    "maxclass": "waveform~",
                    "numinlets": 5,
                    "numoutlets": 6,
                    "patching_rect": [
                        0.0,
                        154.0,
                        520.0,
                        26.0
                    ],
                    "outlettype": [
                        "float",
                        "float",
                        "float",
                        "float",
                        "list",
                        ""
                    ],
                    "parameter_enable": 0,
                    "setmode": 1,
                    "allowdrag": 0
                }
            },
            {
                "box": {
                    "id": "obj-6",
                    "maxclass": "newobj",
                    "numinlets": 0,
                    "numoutlets": 1,
                    "patching_rect": [
                        345.0,
                        220.0,
                        51.0,
                        22.0
                    ],
                    "text": "inlet",
                    "outlettype": [
                        ""
                    ]
                }
            },
            {
                "box": {
                    "id": "obj-7",
                    "maxclass": "newobj",
                    "numinlets": 3,
                    "numoutlets": 3,
                    "patching_rect": [
                        306.5,
                        268.0,
                        128.0,
                        22.0
                    ],
                    "text": "route phase info",
                    "outlettype": [
                        "",
                        "",
                        ""
                    ]
                }
            },
            {
                "box": {
                    "id": "obj-8",
                    "maxclass": "newobj",
                    "numinlets": 2,
                    "numoutlets": 2,
                    "patching_rect": [
                        137.19230769230768,
                        316.0,
                        72.0,
                        22.0
                    ],
                    "text": "route #3",
                    "outlettype": [
                        "",
                        ""
                    ]
                }
            },
            {
                "box": {
                    "id": "obj-9",
                    "maxclass": "newobj",
                    "numinlets": 2,
                    "numoutlets": 2,
                    "patching_rect": [
                        531.8076923076924,
                        316.0,
                        72.0,
                        22.0
                    ],
                    "text": "route #3",
                    "outlettype": [
                        "",
                        ""
                    ]
                }
            },
            {
                "box": {
                    "id": "obj-10",
                    "maxclass": "newobj",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "patching_rect": [
                        113.44131167637141,
                        364.0,
                        65.0,
                        22.0
                    ],
                    "text": "* 1000.",
                    "outlettype": [
                        "float"
                    ]
                }
            },
            {
                "box": {
                    "id": "obj-11",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 7,
                    "patching_rect": [
                        563.5,
                        364.0,
                        184.0,
                        22.0
                    ],
                    "text": "unpack 0. 0 0 0. 0. 0. 0",
                    "outlettype": [
                        "float",
                        "int",
                        "int",
                        "float",
                        "float",
                        "float",
                        "int"
                    ]
                }
            },
            {
                "box": {
                    "id": "obj-12",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "patching_rect": [
                        60.0,
                        412.0,
                        51.0,
                        22.0
                    ],
                    "text": "t f f",
                    "outlettype": [
                        "float",
                        "float"
                    ]
                }
            },
            {
                "box": {
                    "id": "obj-13",
                    "maxclass": "newobj",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "patching_rect": [
                        30.0,
                        460.0,
                        51.0,
                        22.0
                    ],
                    "text": "+ 15.",
                    "outlettype": [
                        "float"
                    ]
                }
            },
            {
                "box": {
                    "id": "obj-14",
                    "maxclass": "newobj",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "patching_rect": [
                        632.5,
                        460.0,
                        86.0,
                        22.0
                    ],
                    "text": "pack 0. 0.",
                    "outlettype": [
                        ""
                    ]
                }
            },
            {
                "box": {
                    "id": "obj-15",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 5,
                    "patching_rect": [
                        329.0,
                        412.0,
                        93.0,
                        22.0
                    ],
                    "text": "sel 0 1 2 3",
                    "outlettype": [
                        "bang",
                        "bang",
                        "bang",
                        "bang",
                        ""
                    ]
                }
            },
            {
                "box": {
                    "id": "obj-16",
                    "maxclass": "newobj",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "patching_rect": [
                        42.5,
                        508.0,
                        86.0,
                        22.0
                    ],
                    "text": "pack 0. 0.",
                    "outlettype": [
                        ""
                    ]
                }
            },
            {
                "box": {
                    "id": "obj-17",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "patching_rect": [
                        149.0,
                        460.0,
                        93.0,
                        22.0
                    ],
                    "text": "set stopped",
                    "outlettype": [
                        ""
                    ]
                }
            },
            {
                "box": {
                    "id": "obj-18",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "patching_rect": [
                        269.0,
                        460.0,
                        93.0,
                        22.0
                    ],
                    "text": "set playing",
                    "outlettype": [
                        ""
                    ]
                }
            },
            {
                "box": {
                    "id": "obj-19",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "patching_rect": [
                        382.0,
                        460.0,
                        107.0,
                        22.0
                    ],
                    "text": "set recording",
                    "outlettype": [
                        ""
                    ]
                }
            },
            {
                "box": {
                    "id": "obj-20",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "patching_rect": [
                        509.0,
                        460.0,
                        93.0,
                        22.0
                    ],
                    "text": "set overdub",
                    "outlettype": [
                        ""
                    ]
                }
            },
            {
                "box": {
                    "id": "obj-21",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "patching_rect": [
                        293.5,
                        556.0,
                        72.0,
                        22.0
                    ],
                    "text": "loadbang",
                    "outlettype": [
                        "bang"
                    ]
                }
            },
            {
                "box": {
                    "id": "obj-22",
                    "maxclass": "newobj",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "patching_rect": [
                        137.0,
                        604.0,
                        65.0,
                        22.0
                    ],
                    "text": "del 500",
                    "outlettype": [
                        "bang"
                    ]
                }
            },
            {
                "box": {
                    "id": "obj-23",
                    "maxclass": "newobj",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "patching_rect": [
                        30.0,
                        604.0,
                        79.0,
                        22.0
                    ],
                    "text": "metro 100",
                    "outlettype": [
                        "bang"
                    ]
                }
            },
            {
                "box": {
                    "id": "obj-24",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "patching_rect": [
                        270.0,
                        652.0,
                        79.0,
                        22.0
                    ],
                    "text": "set #1 #2",
                    "outlettype": [
                        ""
                    ]
                }
            },
            {
                "box": {
                    "id": "obj-25",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "patching_rect": [
                        234.0,
                        604.0,
                        191.0,
                        22.0
                    ],
                    "text": "set voice #3 / channel #2",
                    "outlettype": [
                        ""
                    ]
                }
            },
            {
                "box": {
                    "id": "obj-26",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "patching_rect": [
                        470.0,
                        604.0,
                        79.0,
                        22.0
                    ],
                    "text": "report 30",
                    "outlettype": [
                        ""
                    ]
                }
            },
            {
                "box": {
                    "id": "obj-27",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "patching_rect": [
                        139.6138211382114,
                        652.0,
                        44.0,
                        22.0
                    ],
                    "text": "poll",
                    "outlettype": [
                        ""
                    ]
                }
            },
            {
                "box": {
                    "id": "obj-28",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [
                        260.5,
                        700.0,
                        58.0,
                        22.0
                    ],
                    "text": "outlet",
                    "outlettype": []
                }
            }
        ],
        "lines": [
            {
                "patchline": {
                    "source": [
                        "obj-6",
                        0
                    ],
                    "destination": [
                        "obj-7",
                        0
                    ],
                    "order": 0
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-7",
                        0
                    ],
                    "destination": [
                        "obj-8",
                        0
                    ],
                    "order": 0
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-7",
                        1
                    ],
                    "destination": [
                        "obj-9",
                        0
                    ],
                    "order": 0
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-8",
                        0
                    ],
                    "destination": [
                        "obj-10",
                        0
                    ],
                    "order": 0
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-9",
                        0
                    ],
                    "destination": [
                        "obj-11",
                        0
                    ],
                    "order": 0
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-10",
                        0
                    ],
                    "destination": [
                        "obj-12",
                        0
                    ],
                    "order": 0
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-12",
                        1
                    ],
                    "destination": [
                        "obj-13",
                        0
                    ],
                    "order": 0
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-11",
                        4
                    ],
                    "destination": [
                        "obj-14",
                        1
                    ],
                    "order": 0
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-11",
                        3
                    ],
                    "destination": [
                        "obj-14",
                        0
                    ],
                    "order": 1
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-14",
                        0
                    ],
                    "destination": [
                        "obj-5",
                        2
                    ],
                    "order": 0
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-11",
                        6
                    ],
                    "destination": [
                        "obj-15",
                        0
                    ],
                    "order": 0
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-13",
                        0
                    ],
                    "destination": [
                        "obj-16",
                        1
                    ],
                    "order": 0
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-12",
                        0
                    ],
                    "destination": [
                        "obj-16",
                        0
                    ],
                    "order": 0
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-16",
                        0
                    ],
                    "destination": [
                        "obj-4",
                        2
                    ],
                    "order": 0
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-15",
                        0
                    ],
                    "destination": [
                        "obj-17",
                        0
                    ],
                    "order": 0
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-17",
                        0
                    ],
                    "destination": [
                        "obj-2",
                        0
                    ],
                    "order": 0
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-15",
                        1
                    ],
                    "destination": [
                        "obj-18",
                        0
                    ],
                    "order": 0
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-18",
                        0
                    ],
                    "destination": [
                        "obj-2",
                        0
                    ],
                    "order": 0
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-15",
                        2
                    ],
                    "destination": [
                        "obj-19",
                        0
                    ],
                    "order": 0
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-19",
                        0
                    ],
                    "destination": [
                        "obj-2",
                        0
                    ],
                    "order": 0
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-15",
                        3
                    ],
                    "destination": [
                        "obj-20",
                        0
                    ],
                    "order": 0
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-20",
                        0
                    ],
                    "destination": [
                        "obj-2",
                        0
                    ],
                    "order": 0
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-21",
                        0
                    ],
                    "destination": [
                        "obj-24",
                        0
                    ],
                    "order": 0
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-21",
                        0
                    ],
                    "destination": [
                        "obj-22",
                        0
                    ],
                    "order": 0
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-22",
                        0
                    ],
                    "destination": [
                        "obj-24",
                        0
                    ],
                    "order": 0
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-21",
                        0
                    ],
                    "destination": [
                        "obj-25",
                        0
                    ],
                    "order": 0
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-21",
                        0
                    ],
                    "destination": [
                        "obj-26",
                        0
                    ],
                    "order": 0
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-21",
                        0
                    ],
                    "destination": [
                        "obj-23",
                        0
                    ],
                    "order": 0
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-23",
                        0
                    ],
                    "destination": [
                        "obj-27",
                        0
                    ],
                    "order": 0
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-24",
                        0
                    ],
                    "destination": [
                        "obj-4",
                        0
                    ],
                    "order": 0
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-24",
                        0
                    ],
                    "destination": [
                        "obj-5",
                        0
                    ],
                    "order": 0
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-25",
                        0
                    ],
                    "destination": [
                        "obj-1",
                        0
                    ],
                    "order": 0
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-26",
                        0
                    ],
                    "destination": [
                        "obj-28",
                        0
                    ],
                    "order": 0
                }
            },
            {
                "patchline": {
                    "source": [
                        "obj-27",
                        0
                    ],
                    "destination": [
                        "obj-28",
                        0
                    ],
                    "order": 0
                }
            }
        ],
        "dependency_cache": [],
        "autosave": 0
    }
}