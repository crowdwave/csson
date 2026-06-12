# CSSON samples

50 example files spanning many domains, from the trivial
(`hello-minimal-csson.css`) to deeply-nested, schema-typed configs. Every file is
valid CSSON v1 — confirmed with `csson check` — and demonstrates that CSSON is
just CSS used as data: records are blocks, fields are `--custom-properties`,
repeated sibling blocks become arrays, and `@property` blocks carry schema.

They use a **`.css` extension** (named `*-csson.css`) because CSSON *is* valid
CSS — a browser will load any of these as a stylesheet when served as
`text/css`, and CSS-aware tooling treats them as first-class CSS.

Read any of them:
```sh
csson canon samples/spacecraft-mission-csson.css    # → canonical JSON
csson check samples/spacecraft-mission-csson.css    # validate (exit 0 = valid)
```

## Infrastructure & systems
| File | What it models |
|---|---|
| `web-server-csson.css` | reverse proxy + virtual hosts |
| `kubernetes-deployment-csson.css` | a Deployment: containers, probes, resources |
| `ci-pipeline-csson.css` | CI/CD stages, jobs, steps, matrix |
| `dns-zone-csson.css` | DNS zone with repeated A/AAAA/MX/TXT records |
| `firewall-rules-csson.css` | ingress/egress security-group rules |
| `cdn-edge-csson.css` | CDN cache rules, origins, geo routing |
| `database-schema-csson.css` | relational tables, columns, indexes (+`@property`) |
| `message-queue-csson.css` | broker topics, partitions, consumers |
| `load-balancer-csson.css` | L4/L7 frontends, backends, health checks |
| `observability-stack-csson.css` | scrape targets, alert rules (+`@property`) |

## Games, media & audio-visual
| File | What it models |
|---|---|
| `rpg-character-csson.css` | character sheet: stats, inventory, abilities (+`@property`) |
| `board-game-rules-csson.css` | pieces, phases, win conditions |
| `retro-emulator-csson.css` | video/audio/input profile |
| `esports-tournament-csson.css` | teams, bracket, match schedule |
| `synth-patch-csson.css` | oscillators, envelopes, LFOs (Hz/ms) |
| `3d-scene-csson.css` | camera, lights, meshes, materials (+`@property`) |
| `podcast-feed-csson.css` | show + episodes |
| `movie-shotlist-csson.css` | scenes, shots, lenses, cast |
| `theme-park-csson.css` | zones, rides, schedule |
| `orchestra-csson.css` | sections, seating, program (`& child` nesting) |

## Space, aviation & transport
| File | What it models |
|---|---|
| `spacecraft-mission-csson.css` | mission phases, instruments, delta-v (+`@property`) |
| `satellite-constellation-csson.css` | orbital planes, ground stations |
| `flight-plan-csson.css` | route waypoints, fuel, crew, alternates |
| `railway-timetable-csson.css` | lines, stations, trains, fares |
| `drone-mission-csson.css` | waypoints, geofence, failsafes |
| `fleet-logistics-csson.css` | vehicles, routes, stops, drivers |
| `submarine-systems-csson.css` | ballast, sonar, depth limits (+`@property`) |
| `mars-rover-csson.css` | instruments, power, mobility, sol schedule |
| `traffic-intersection-csson.css` | approaches, signal phases, detectors |
| `cargo-ship-csson.css` | holds, ports of call (`& child` nesting) |

## Science, energy & industry
| File | What it models |
|---|---|
| `weather-station-csson.css` | sensors, thresholds, calibration (+`@property`) |
| `solar-power-plant-csson.css` | arrays, inverters, tracking, storage |
| `wind-turbine-farm-csson.css` | turbines, wind profile, SCADA |
| `particle-detector-csson.css` | tracker/calorimeter/muon subsystems |
| `robot-arm-csson.css` | 6-axis joints, kinematics, safety (+`@property`) |
| `greenhouse-automation-csson.css` | zones, climate, irrigation, lighting |
| `chemistry-lab-csson.css` | reagents, reaction steps, safety |
| `ml-hyperparameters-csson.css` | layers, optimizer, data splits |
| `nuclear-reactor-csson.css` | core, control rods, coolant, SCRAM |
| `genome-pipeline-csson.css` | stages, reference, thresholds (`& child` nesting) |

## Everyday, business & home
| File | What it models |
|---|---|
| `ecommerce-catalog-csson.css` | products, categories, shipping |
| `financial-portfolio-csson.css` | holdings, accounts, allocations (+`@property`) |
| `recipe-lasagna-csson.css` | ingredients, steps, nutrition |
| `brewery-recipe-csson.css` | malts, hops, mash schedule, targets |
| `nutrition-plan-csson.css` | weekly days → meals → macros |
| `library-catalog-csson.css` | books, members, loan rules |
| `hospital-ward-csson.css` | beds, staff, medication schedule (+`@property`) |
| `design-tokens-csson.css` | colors (hex + oklch), spacing, typography |
| `smart-home-csson.css` | rooms, devices, scenes (`& child` nesting) |
| `hello-minimal-csson.css` | the simplest valid CSSON — start here |

## Features exercised across the set
Integers → JSON numbers; quoted strings; unit values (`px`, `rem`, `s`, `ms`,
`Hz`, `deg`, `turn`, `%`, `km`, `m`, `kg`, …) and colors (`#hex`, `oklch(…)`)
and `url(…)` as verbatim string scalars; comma arrays; space-separated tuples;
repeated-sibling arrays; 3–4 levels of nesting; comments in every file; 10
`@property` schema blocks; and 5 files using the `& child` CSS-nesting form.
