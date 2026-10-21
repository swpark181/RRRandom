// Download a Mixamo character's RRRandom animation set (paste into the mixamo.com page's console / page JS, logged in).
// The character is the one Mixamo holds now (it keeps only the last upload). Files land in Downloads as
// <prefix>_<name>.fbx: SK with skin, the rest without; run_* in place. Mixamo answers 429 if pushed: 4 s between exports.
// Usage: window.__runAll('dragon');  progress: window.__prog;  just some: window.__runAll('dragon', ['SK', 'walk'])
window.__H = {Authorization: 'Bearer ' + localStorage.getItem('access_token'), 'X-Api-Key': 'mixamo2',
  Accept: 'application/json', 'Content-Type': 'application/json'};
const S = '-b96c-11e4-a802-0aaa78deedf9';
// [product id, file name, with skin, in place] — the fox's set (HANDOFF 9차, 23차, 24차)
window.__LIST = [
  ['c9cd4ed4' + S, 'SK', 'true', false],            // Two Hand Rifle Idle (the skinned rig)
  ['c9c9067d' + S, 'walk', 'false', false],         // Quickly Walk Forward With Rifle
  ['c9c94dfa' + S, 'fire', 'false', false],         // Firing A Rifle While Standing
  ['c9cd74a2' + S, 'death', 'false', false],        // Rifle Getting Hit To Ground
  ['c9c6e5b1' + S, 'victory', 'false', false],      // House Dance Variation Two
  ['c9c84492' + S, 'aim_idle', 'false', false],
  ['c9ce8798' + S, 'run_f', 'false', true],
  ['c9ce8b7b' + S, 'run_b', 'false', true],
  ['c9ce8c37' + S, 'run_l', 'false', true],
  ['c9ce8cf6' + S, 'run_r', 'false', true],
  ['c9c65aac' + S, 'dance1_twist', 'false', false],
  ['c9c6e3a5' + S, 'dance2_shimmy', 'false', false],
  ['c9cef3e2' + S, 'dance3_runningman', 'false', false],
  ['c9cd46a9' + S, 'dance4_chicken', 'false', false],
  ['c9c70794' + S, 'defeat1_defeated', 'false', false],
  ['c9c984ee' + S, 'defeat2_crying', 'false', false],
  ['c9cce12b' + S, 'defeat3_sad', 'false', false],
  ['c9cdb4c8' + S, 'flyback', 'false', false],      // Flying Back Death (24차)
];
// Extra motions for one species' ultimate (round 29/25): window.__runAll('rabbit', null, window.__EXTRA.rabbit)
window.__EXTRA = {
  rabbit: [['c9c9d919' + S, 'jumpup', 'false', false], ['c9cab793' + S, 'frontflip', 'false', false],
           ['c9c991b1' + S, 'fallidle', 'false', false], ['c9c99c82' + S, 'hardlanding', 'false', false]],
  tiger: [['c9ccb4c2' + S, 'roar', 'false', false], ['c9c95db7' + S, 'jumpattack', 'false', false]],  // Mutant Roaring, Mutant Jump Attack
};
window.__runAll = async (prefix, only, list) => {
  window.__prog = [];
  const H = window.__H, sleep = ms => new Promise(r => setTimeout(r, ms));
  const primary = await fetch('/api/v1/characters/primary', {headers: H}).then(r => r.json());
  const CID = window.__CID = primary.primary_character_id;
  window.__prog.push('character ' + primary.primary_character_name);
  for (const [pid, name, skin, inplace] of (list || window.__LIST)) {
    if (only && !only.includes(name)) continue;
    let tries = 0, done = false;
    while (!done && tries < 4) {
      tries++;
      try {
        const prod = await fetch(`/api/v1/products/${pid}?similar=0&character_id=${CID}`, {headers: H}).then(r => r.json());
        const g = prod.details.gms_hash;
        const body = {character_id: CID, product_name: prefix + '_' + name, type: 'Motion',
          gms_hash: [{'model-id': g['model-id'], mirror: false, trim: g.trim, overdrive: 0, params: g.params.map(p => p[1]).join(','),
            'arm-space': g['arm-space'], inplace: inplace && !!prod.details.supports_inplace}],
          preferences: {format: 'fbx7_2019', skin: skin, fps: '30', reducekf: '0'}};
        const res = await fetch('/api/v1/animations/export', {method: 'POST', headers: H, body: JSON.stringify(body)});
        if (!res.ok) { window.__prog.push(name + ' export ' + res.status + ' try ' + tries); await sleep(15000); continue; }
        for (let i = 0; i < 90 && !done; i++) {
          await sleep(2500);
          const m = await fetch(`/api/v1/characters/${CID}/monitor`, {headers: H}).then(r => r.json());
          if (m.status === 'completed') {
            // The result is on another host (no CORS for fetch): a link click downloads it
            const a = document.createElement('a'); a.href = m.job_result; a.download = prefix + '_' + name + '.fbx';
            document.body.appendChild(a); a.click(); a.remove();
            window.__prog.push(name + ' ok'); done = true;
          } else if (m.status === 'failed') { window.__prog.push(name + ' failed ' + JSON.stringify(m).slice(0, 150)); break; }
        }
      } catch (e) { window.__prog.push(name + ' error ' + e + ' try ' + tries); await sleep(10000); }
    }
    await sleep(4000);
  }
  window.__prog.push('ALL DONE');
};
'ready';
