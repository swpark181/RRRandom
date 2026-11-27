import unreal
# Round 103: navmesh paths along the blue sky path's stairs (after the navmesh is built), logged as NAVCHECK
st = {'t': 0}
def tick(dt):
    st['t'] += dt
    if st['t'] < 20 or st.get('done'):
        return
    st['done'] = True
    worlds = [w for w in unreal.ObjectIterator(unreal.World) if unreal.GameplayStatics.get_player_controller(w, 0)]
    w = worlds[-1]
    links = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.NavLinkProxy)
    unreal.log(f'NAVCHECK link actors {len(links)}')
    for a, b in [((600, -2200, 10), (600, -2550, 210)), ((600, -2200, 10), (600, -3150, 810)), ((600, -2200, 10), (-750, -3150, 1110)), ((600,-2550,210),(600,-2750,410))]:
        p = unreal.NavigationSystemV1.find_path_to_location_synchronously(w, unreal.Vector(*a), unreal.Vector(*b))
        pts = [f'({q.x:.0f},{q.y:.0f},{q.z:.0f})' for q in p.path_points] if p else []
        unreal.log(f'NAVCHECK {a} -> {b}: partial={p.is_partial() if p else None} {" ".join(pts)}')
    unreal.SystemLibrary.quit_game(w, None, unreal.QuitPreference.QUIT, False)
unreal.register_slate_post_tick_callback(tick)
