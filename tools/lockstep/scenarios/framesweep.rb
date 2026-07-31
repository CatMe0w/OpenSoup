# Orientation -> sprite rotation frame. Dense around phase boundaries.
# Must call $e.render before reading: the original recomputes in the render pass.
$e = $default_engine
$e.scene_bottom_left = Vector[0.0, 0.0]
$e.scene_top_right = Vector[-12.8, -8.0]
$e.scene_walls_changed
$r = RectangleThin48.new; $e.toys << $r; $b = $r.limbs.to_a.first
$b.gravity_override = 0.0; $b.air_resistance_linear = 0.0
$sp = $b.sprites.to_a.first
$b.position = Vector[4.0, 3.0]
puts format("N %d", $sp.frame_count)
$b.orientation = 0
$e.render; puts format("F000 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = -9.9999999999999995e-07
$e.render; puts format("F001 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 9.9999999999999995e-07
$e.render; puts format("F002 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 0.032724923474893676
$e.render; puts format("F003 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 0.065449846949787352
$e.render; puts format("F004 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 0.065448846949787351
$e.render; puts format("F005 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 0.065450846949787353
$e.render; puts format("F006 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 0.098174770424681035
$e.render; puts format("F007 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 1.5053464798451091
$e.render; puts format("F008 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 1.5053454798451091
$e.render; puts format("F009 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 1.505347479845109
$e.render; puts format("F010 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 1.5380714033200027
$e.render; puts format("F011 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 1.5707963267948966
$e.render; puts format("F012 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 1.5707953267948966
$e.render; puts format("F013 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 1.5707973267948965
$e.render; puts format("F014 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 1.6035212502697902
$e.render; puts format("F015 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 3.0761428066400054
$e.render; puts format("F016 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 3.0761418066400052
$e.render; puts format("F017 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 3.0761438066400055
$e.render; puts format("F018 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 3.1088677301148993
$e.render; puts format("F019 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 3.1415926535897931
$e.render; puts format("F020 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 3.141591653589793
$e.render; puts format("F021 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 3.1415936535897933
$e.render; puts format("F022 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 3.1743175770646865
$e.render; puts format("F023 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 4.6469391334349019
$e.render; puts format("F024 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 4.6469381334349018
$e.render; puts format("F025 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 4.6469401334349021
$e.render; puts format("F026 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 4.6796640569097958
$e.render; puts format("F027 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 4.7123889803846897
$e.render; puts format("F028 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 4.7123879803846895
$e.render; puts format("F029 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 4.7123899803846898
$e.render; puts format("F030 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 4.7451139038595826
$e.render; puts format("F031 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 6.2177354602297985
$e.render; puts format("F032 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 6.2177344602297984
$e.render; puts format("F033 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 6.2177364602297986
$e.render; puts format("F034 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 6.2504603837046924
$e.render; puts format("F035 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 0.032724913209676743
$e.render; puts format("F036 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 0.032724916934967041
$e.render; puts format("F037 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 0.032724920660257339
$e.render; puts format("F038 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 0.032724924385547638
$e.render; puts format("F039 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 0.032724928110837936
$e.render; puts format("F040 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 0.032724931836128235
$e.render; puts format("F041 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 0.032724935561418533
$e.render; puts format("F042 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 1.5380710363388062
$e.render; puts format("F043 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 1.5380711555480957
$e.render; puts format("F044 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 1.5380712747573853
$e.render; puts format("F045 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 1.5380713939666748
$e.render; puts format("F046 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 1.5380715131759644
$e.render; puts format("F047 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 1.5380716323852539
$e.render; puts format("F048 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 1.5380717515945435
$e.render; puts format("F049 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 3.1743168830871582
$e.render; puts format("F050 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 3.1743171215057373
$e.render; puts format("F051 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 3.1743173599243164
$e.render; puts format("F052 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 3.1743175983428955
$e.render; puts format("F053 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 3.1743178367614746
$e.render; puts format("F054 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 3.1743180751800537
$e.render; puts format("F055 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 3.1743183135986328
$e.render; puts format("F056 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 4.745112419128418
$e.render; puts format("F057 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 4.7451128959655762
$e.render; puts format("F058 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 4.7451133728027344
$e.render; puts format("F059 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 4.7451138496398926
$e.render; puts format("F060 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 4.7451143264770508
$e.render; puts format("F061 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 4.745114803314209
$e.render; puts format("F062 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 4.7451152801513672
$e.render; puts format("F063 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 6.2504587173461914
$e.render; puts format("F064 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 6.2504591941833496
$e.render; puts format("F065 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 6.2504596710205078
$e.render; puts format("F066 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 6.250460147857666
$e.render; puts format("F067 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 6.2504606246948242
$e.render; puts format("F068 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 6.2504611015319824
$e.render; puts format("F069 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 6.2504615783691406
$e.render; puts format("F070 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 0
$e.render; puts format("F071 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 3.1415926535897931
$e.render; puts format("F072 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = -3.1415926535897931
$e.render; puts format("F073 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 7.8539816339744828
$e.render; puts format("F074 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = -7.8539816339744828
$e.render; puts format("F075 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 15.707963267948966
$e.render; puts format("F076 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = -15.707963267948966
$e.render; puts format("F077 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = -8.8083617583418814
$e.render; puts format("F078 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = -17.457541303774903
$e.render; puts format("F079 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 7.5467236519926857
$e.render; puts format("F080 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = -21.378185666622862
$e.render; puts format("F081 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 1.7941002153344598
$e.render; puts format("F082 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = -6.7155541543707216
$e.render; puts format("F083 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = -22.100053761264661
$e.render; puts format("F084 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 0.37178665947101308
$e.render; puts format("F085 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = -23.125217077900757
$e.render; puts format("F086 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = -3.3177158168807068
$e.render; puts format("F087 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = -21.507228821269052
$e.render; puts format("F088 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = -20.464349332806748
$e.render; puts format("F089 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = -3.7740405428743031
$e.render; puts format("F090 %+.9e %d", $b.orientation, $sp.frame)
$b.orientation = 16.342606233601906
$e.render; puts format("F091 %+.9e %d", $b.orientation, $sp.frame)
