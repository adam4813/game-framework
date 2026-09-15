#pragma once

#include <string_view>

inline constexpr std::string_view kTestSceneJson = R"json(
{
  "entities": [
    {
      "name": "TestCamera",
      "components": {
        "transform": {
          "position": [0.0, 5.0, 5.0],
          "rotation": [0.0, 0.0, 0.0],
          "scale": [1.0, 1.0, 1.0]
        },
        "camera": {
          "up": [0.0, 1.0, 0.0],
          "fov": 60.0,
          "aspect_ratio": 1.7777778,
          "near": 0.1,
          "far": 100.0
        }
      }
    },
    {
      "name": "TestLight",
      "components": {
        "transform": {
          "position": [0.0, 10.0, 0.0],
          "rotation": [0.0, 0.0, 0.0],
          "scale": [1.0, 1.0, 1.0]
        },
        "directional_light": {
          "direction": [-0.55, -1.0, -0.4],
          "color": [255, 245, 220, 255],
          "intensity": 1.0,
          "specular_strength": 0.35,
          "shininess": 24.0,
          "casts_shadows": true,
          "shadow_ground_y": 0.0,
          "shadow_color": [8, 10, 14, 115]
        }
      }
    },
    {
      "name": "TestBox",
      "components": {
        "transform": {
          "position": [0.0, 3.0, 0.0],
          "rotation": [0.0, 0.0, 0.0],
          "scale": [1.0, 1.0, 1.0]
        },
        "cube": {
          "size": [1.0, 1.0, 1.0]
        },
        "collision_shape": {
          "type": "box",
          "box_half_extents": [0.5, 0.5, 0.5]
        },
        "rigid_body": {
          "motion_type": "dynamic",
          "mass": 1.0,
          "linear_damping": 0.05,
          "angular_damping": 0.05,
          "friction": 0.5,
          "restitution": 0.3,
          "enable_ccd": false,
          "use_gravity": true,
          "gravity_scale": 1.0
        }
      }
    },
    {
      "name": "TestSphere",
      "components": {
        "transform": {
          "position": [-2.5, 0.5, 0.0],
          "rotation": [0.0, 0.0, 0.0],
          "scale": [1.0, 1.0, 1.0]
        },
        "sphere": {
          "radius": 0.5
        }
      }
    },
    {
      "name": "TestTimer",
      "components": {
        "timer": {
          "remaining": 5.0,
          "duration": 5.0,
          "repeat": false,
          "expired": false
        },
        "cooldown": {
          "remaining": 2.0,
          "duration": 2.0
        }
      }
    },
    {
      "name": "TestGraphNode",
      "components": {
        "graph_node": {
          "label": "MainNode",
          "order": 1
        }
      }
    },
    {
      "name": "TestGraphEdge",
      "components": {
        "graph_edge_weight": {
          "weight": 0.75,
          "tag": "strong"
        }
      }
    },
    {
      "name": "TestSteering",
      "components": {
        "transform": {
          "position": [7.0, 0.0, 0.0],
          "rotation": [0.0, 0.0, 0.0],
          "scale": [1.0, 1.0, 1.0]
        },
        "steering_agent": {
          "max_speed": 5.0,
          "max_force": 10.0,
          "mass": 1.5
        },
        "steering_target": {
          "position": [1.0, 0.0, 3.0]
        }
      }
    },
    {
      "name": "TestFSMAgent",
      "components": {
        "fsm_agent": {
          "initial_state": "idle"
        }
      }
    },
    {
      "name": "TestGOAPAgent",
      "components": {
        "goap_agent": {}
      }
    },
    {
      "name": "TestBTree",
      "components": {
        "behavior_tree": {
          "tick_interval": 0.1
        }
      }
    },
    {
      "name": "TestUtility",
      "components": {
        "utility_agent": {}
      }
    },
    {
      "name": "TestGrid",
      "components": {
        "grid_position": { "x": 2, "z": 3 }
      }
    },
    {
      "name": "TestQuad",
      "components": {
        "transform": {
          "position": [3.0, 0.0, 0.0],
          "rotation": [0.0, 0.0, 0.0],
          "scale": [1.0, 1.0, 1.0]
        },
        "quad": {
          "size": [2.0, 3.0]
        }
      }
    },
    {
      "name": "TestCapsule",
      "components": {
        "transform": {
          "position": [5.0, 0.0, 0.0],
          "rotation": [0.0, 0.0, 0.0],
          "scale": [1.0, 1.0, 1.0]
        },
        "capsule": {
          "radius": 0.4,
          "height": 1.5
        }
      }
    },
    {
      "name": "TestParticles",
      "components": {
        "transform": {
          "position": [0.0, 0.0, 5.0],
          "rotation": [0.0, 0.0, 0.0],
          "scale": [1.0, 1.0, 1.0]
        },
        "particle_emitter": {
          "rate": 30.0,
          "lifetime": 0.6,
          "speed": 2.5,
          "start_size": 0.08,
          "spread": 0.6,
          "direction": [0.0, 1.0, 0.0],
          "color_start": [255, 200, 80, 255],
          "color_end": [255, 80, 30, 0],
          "emitting": false
        }
      }
    },
    {
      "name": "TestTween",
      "components": {
        "tween": {
          "elapsed": 0.0,
          "duration": 2.0,
          "from": 0.0,
          "to": 1.0,
          "easing": 0,
          "value": 0.0,
          "done": false
        }
      }
    },
    {
      "name": "TestMaterial",
      "components": {
        "material": {
          "color": [200, 100, 50, 255],
          "wireframe": false,
          "cast_shadow": true
        }
      }
    },
    {
      "name": "TestVelocity",
      "components": {
        "transform": {
          "position": [0.0, 0.0, -3.0],
          "rotation": [0.0, 0.0, 0.0],
          "scale": [1.0, 1.0, 1.0]
        },
        "physics_velocity": {
          "linear": [1.0, 0.0, 0.0],
          "angular": [0.0, 0.5, 0.0]
        }
      }
    },
    {
      "name": "TestShape",
      "components": {
        "transform": {
          "position": [-3.0, 0.0, 0.0],
          "rotation": [0.0, 0.0, 0.0],
          "scale": [1.0, 1.0, 1.0]
        },
        "collision_shape": {
          "type": "box",
          "offset": [0.0, 0.1, 0.0]
        }
      }
    },
    {
      "name": "TestAudio",
      "components": {
        "sound_effect": {
          "path": "audio/sfx/jump.wav"
        }
      }
    },
    {
      "name": "TestMesh",
      "components": {
        "mesh": {
          "path": "models/cube.glb"
        }
      }
    },
    {
      "name": "TestAlbedo",
      "components": {
        "albedo": {
          "path": "textures/checker.png"
        }
      }
    },
    {
      "name": "TestMusic",
      "components": {
        "music": {
          "path": "audio/music/theme.ogg",
          "loop": true
        }
      }
    },
    {
      "name": "TestTilemapRegistry",
      "components": {
        "tileset": {
          "tiles": {},
          "name_to_id": {}
        }
      }
    },
    {
      "name": "TestTilemapAtlas",
      "components": {
        "sprite_atlas": {
          "texture_path": "textures/tileset.png",
          "image_width_px": 128,
          "image_height_px": 128
        },
        "material": {
          "color": [255, 255, 255, 255],
          "wireframe": false,
          "cast_shadow": true
        }
      }
    },
    {
      "name": "TestTilemap",
      "components": {
        "tilemap": {
          "width": 10,
          "height": 10,
          "tile_size_px": 16,
          "pixels_per_world_unit": 32.0,
          "tile_ids": [0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                       0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                       0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                       0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                       0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                       0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                       0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                       0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                       0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                       0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
        }
      },
      "refs": {
        "use_registry": "TestTilemapRegistry",
        "use_sprite_atlas": "TestTilemapAtlas"
      }
    },
    {
      "name": "TestPathAgent",
      "components": {
        "path_agent": {
          "arrival_radius": 0.5
        }
      }
    },
    {
      "name": "TestPathFollow",
      "components": {
        "path_follow": {
          "speed": 4.0
        }
      }
    },
    {
      "name": "TestPathAutoReplan",
      "components": {
        "path_auto_replan": {
          "goal": [3.0, 0.0, 5.0],
          "interval": 0.5
        }
      }
    }
  ]
}
)json";

inline constexpr std::string_view kTestSceneScript = R"as(
void OnInit(Entity self) {
    Entity camera = CreateNamedEntity("TestCamera");
    Transform@ ct = camera.AddTransform();
    ct.position = vec3(0.0f, 5.0f, 5.0f);
    ct.rotation = vec3(0.0f, 0.0f, 0.0f);
    ct.scale = vec3(1.0f, 1.0f, 1.0f);
    Camera@ cc = camera.AddCamera();
    cc.up = vec3(0.0f, 1.0f, 0.0f);
    cc.fov = 60.0f;
    cc.aspect_ratio = 1.7777778f;
    cc.near_plane = 0.1f;
    cc.far_plane = 100.0f;

    Entity light = CreateNamedEntity("TestLight");
    Transform@ lt = light.AddTransform();
    lt.position = vec3(0.0f, 10.0f, 0.0f);
    lt.rotation = vec3(0.0f, 0.0f, 0.0f);
    lt.scale = vec3(1.0f, 1.0f, 1.0f);
    DirectionalLight@ dl = light.AddDirectionalLight();
    dl.direction = vec3(-0.55f, -1.0f, -0.4f);
    dl.color = MakeRgba(255, 245, 220, 255);
    dl.intensity = 1.0f;
    dl.specular_strength = 0.35f;
    dl.shininess = 24.0f;
    dl.casts_shadows = true;
    dl.shadow_ground_y = 0.0f;
    dl.shadow_color = MakeRgba(8, 10, 14, 115);

    Entity box = CreateNamedEntity("TestBox");
    Transform@ bt = box.AddTransform();
    bt.position = vec3(0.0f, 3.0f, 0.0f);
    bt.rotation = vec3(0.0f, 0.0f, 0.0f);
    bt.scale = vec3(1.0f, 1.0f, 1.0f);
    CubePrimitive@ bc = box.AddCubePrimitive();
    bc.size = vec3(1.0f, 1.0f, 1.0f);
    // CollisionShape must be added before RigidBody so the Jolt OnSet<RigidBody> observer finds it.
    CollisionShape@ bcs = box.AddCollisionShape();
    bcs.offset = vec3(0.0f, 0.0f, 0.0f);
    RigidBody@ br = box.AddRigidBody();
    br.mass = 1.0f;
    br.friction = 0.5f;
    br.restitution = 0.3f;
    br.use_gravity = true;
    br.gravity_scale = 1.0f;

    Entity sphere = CreateNamedEntity("TestSphere");
    Transform@ st = sphere.AddTransform();
    st.position = vec3(-2.5f, 0.5f, 0.0f);
    st.rotation = vec3(0.0f, 0.0f, 0.0f);
    st.scale = vec3(1.0f, 1.0f, 1.0f);
    SpherePrimitive@ ss = sphere.AddSpherePrimitive();
    ss.radius = 0.5f;

    Entity timer_ent = CreateNamedEntity("TestTimer");
    Timer@ tt = timer_ent.AddTimer();
    tt.remaining = 5.0f;
    tt.duration = 5.0f;
    tt.repeat = false;
    tt.expired = false;
    Cooldown@ tc = timer_ent.AddCooldown();
    tc.remaining = 2.0f;
    tc.duration = 2.0f;

    Entity graph_node_ent = CreateNamedEntity("TestGraphNode");
    GraphNode@ gn = graph_node_ent.AddGraphNode();
    gn.label = "MainNode";
    gn.order = 1;

    Entity graph_edge_ent = CreateNamedEntity("TestGraphEdge");
    GraphEdgeWeight@ gew = graph_edge_ent.AddGraphEdgeWeight();
    gew.weight = 0.75f;
    gew.tag = "strong";

    Entity steering_ent = CreateNamedEntity("TestSteering");
    Transform@ stt = steering_ent.AddTransform();
    stt.position = vec3(7.0f, 0.0f, 0.0f);
    stt.rotation = vec3(0.0f, 0.0f, 0.0f);
    stt.scale = vec3(1.0f, 1.0f, 1.0f);
    SteeringAgent@ sa = steering_ent.AddSteeringAgent();
    sa.max_speed = 5.0f;
    sa.max_force = 10.0f;
    sa.mass = 1.5f;
    SteeringTarget@ stg = steering_ent.AddSteeringTarget();
    stg.position = vec3(1.0f, 0.0f, 3.0f);

    Entity fsm_ent = CreateNamedEntity("TestFSMAgent");
    FSMAgent@ fsm = fsm_ent.AddFSMAgent();
    fsm.current_state = "idle";

    Entity goap_ent = CreateNamedEntity("TestGOAPAgent");
    GOAPAgent@ goap = goap_ent.AddGOAPAgent();
    goap.needs_replan = true;

    Entity bt_ent = CreateNamedEntity("TestBTree");
    BehaviorTree@ btree = bt_ent.AddBehaviorTree();
    btree.tick_interval = 0.1f;

    Entity utility_ent = CreateNamedEntity("TestUtility");
    utility_ent.AddUtilityAgent();

    Entity grid_ent = CreateNamedEntity("TestGrid");
    GridPosition@ gp = grid_ent.AddGridPosition();
    gp.x = 2;
    gp.z = 3;

    Entity quad = CreateNamedEntity("TestQuad");
    Transform@ qt = quad.AddTransform();
    qt.position = vec3(3.0f, 0.0f, 0.0f);
    qt.rotation = vec3(0.0f, 0.0f, 0.0f);
    qt.scale = vec3(1.0f, 1.0f, 1.0f);
    QuadPrimitive@ qp = quad.AddQuadPrimitive();
    qp.size = vec2(2.0f, 3.0f);

    Entity capsule = CreateNamedEntity("TestCapsule");
    Transform@ cat = capsule.AddTransform();
    cat.position = vec3(5.0f, 0.0f, 0.0f);
    cat.rotation = vec3(0.0f, 0.0f, 0.0f);
    cat.scale = vec3(1.0f, 1.0f, 1.0f);
    CapsulePrimitive@ cp = capsule.AddCapsulePrimitive();
    cp.radius = 0.4f;
    cp.height = 1.5f;

    Entity particles = CreateNamedEntity("TestParticles");
    Transform@ pt = particles.AddTransform();
    pt.position = vec3(0.0f, 0.0f, 5.0f);
    pt.rotation = vec3(0.0f, 0.0f, 0.0f);
    pt.scale = vec3(1.0f, 1.0f, 1.0f);
    ParticleEmitter@ pe = particles.AddParticleEmitter();
    pe.rate = 30.0f;
    pe.particleLifetime = 0.6f;
    pe.speed = 2.5f;
    pe.startSize = 0.08f;
    pe.spread = 0.6f;
    pe.direction = vec3(0.0f, 1.0f, 0.0f);
    pe.colorStart = MakeRgba(255, 200, 80, 255);
    pe.colorEnd = MakeRgba(255, 80, 30, 0);
    pe.emitting = false;

    Entity tween_ent = CreateNamedEntity("TestTween");
    Tween@ tw = tween_ent.AddTween();
    tw.elapsed = 0.0f;
    tw.duration = 2.0f;
    tw.from = 0.0f;
    tw.to = 1.0f;
    tw.easing = Easing_Linear;
    tw.value = 0.0f;
    tw.done = false;

    Entity material_ent = CreateNamedEntity("TestMaterial");
    Material@ mat = material_ent.AddMaterial();
    mat.color = MakeRgba(200, 100, 50, 255);
    mat.wireframe = false;
    mat.cast_shadow = true;

    Entity velocity = CreateNamedEntity("TestVelocity");
    Transform@ vt = velocity.AddTransform();
    vt.position = vec3(0.0f, 0.0f, -3.0f);
    vt.rotation = vec3(0.0f, 0.0f, 0.0f);
    vt.scale = vec3(1.0f, 1.0f, 1.0f);
    PhysicsVelocity@ pv = velocity.AddPhysicsVelocity();
    pv.linear = vec3(1.0f, 0.0f, 0.0f);
    pv.angular = vec3(0.0f, 0.5f, 0.0f);

    Entity shape = CreateNamedEntity("TestShape");
    Transform@ sht = shape.AddTransform();
    sht.position = vec3(-3.0f, 0.0f, 0.0f);
    sht.rotation = vec3(0.0f, 0.0f, 0.0f);
    sht.scale = vec3(1.0f, 1.0f, 1.0f);
    CollisionShape@ cs = shape.AddCollisionShape();
    cs.offset = vec3(0.0f, 0.1f, 0.0f);

    Entity audio = CreateNamedEntity("TestAudio");
    SoundEffect@ sfx = audio.AddSoundEffect();
    sfx.path = "./audio/sfx/jump.wav";

    Entity mesh_ent = CreateNamedEntity("TestMesh");
    MeshPrimitive@ mp = mesh_ent.AddMeshPrimitive();
    mp.path = "./models/cube.glb";

    Entity albedo_ent = CreateNamedEntity("TestAlbedo");
    AlbedoMap@ am = albedo_ent.AddAlbedoMap();
    am.path = "./textures/checker.png";

    Entity music_ent = CreateNamedEntity("TestMusic");
    Music@ mus = music_ent.AddMusic();
    mus.path = "./audio/music/theme.ogg";
    mus.loop = true;

    Entity tm_registry = CreateNamedEntity("TestTilemapRegistry");
    tm_registry.AddTileSet();

    Entity tm_atlas = CreateNamedEntity("TestTilemapAtlas");
    SpriteAtlas@ atlas_sa = tm_atlas.AddSpriteAtlas();
    atlas_sa.texture_path = "./textures/tileset.png";
    atlas_sa.image_width_px = 128;
    atlas_sa.image_height_px = 128;
    Material@ atlas_mat = tm_atlas.AddMaterial();
    atlas_mat.color = MakeRgba(255, 255, 255, 255);
    atlas_mat.cast_shadow = true;

    Entity tilemap = CreateNamedEntity("TestTilemap");
    tilemap.AddTilemap();

    Entity path_agent_ent = CreateNamedEntity("TestPathAgent");
    PathAgent@ pa = path_agent_ent.AddPathAgent();
    pa.arrival_radius = 0.5f;

    Entity path_follow_ent = CreateNamedEntity("TestPathFollow");
    PathFollow@ pf = path_follow_ent.AddPathFollow();
    pf.speed = 4.0f;

    Entity path_replan_ent = CreateNamedEntity("TestPathAutoReplan");
    PathAutoReplan@ pr = path_replan_ent.AddPathAutoReplan();
    pr.goal = vec3(3.0f, 0.0f, 5.0f);
    pr.interval = 0.5f;
}
)as";
