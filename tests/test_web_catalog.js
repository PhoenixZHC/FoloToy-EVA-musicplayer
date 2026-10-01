const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

class Element {
  constructor() { this.children = []; this.style = {}; this.files = []; this._text = ''; }
  set textContent(value) { this._text = value; this.children = []; }
  get textContent() { return this._text; }
  append(...children) { this.children.push(...children); }
  appendChild(child) { this.children.push(child); return child; }
}

async function main() {
  const html = fs.readFileSync(path.join(__dirname, '../main/web_ui.html'), 'utf8');
  const scripts = [...html.matchAll(/<script>([\s\S]*?)<\/script>/g)];
  const source = scripts[1][1].replace('/*__EVA_FONT_CODES__*/', '[]');
  const elements = new Map();
  const element = id => {
    if (!elements.has(id)) elements.set(id, new Element());
    return elements.get(id);
  };
  let revision = 'session-1';
  let titles = ['A', 'B', 'C'];
  let gets = 0;
  const posts = [];
  const sandbox = {
    document: { getElementById: element, createElement: () => new Element(),
      fonts: { load: async () => [{}] } },
    confirm: () => true,
    FoloAudio: require('../main/audio_adpcm.js'),
    Uint8Array,
    fetch: async (url, options) => {
      if (url === '/audio/list') {
        gets++;
        return { ok: true, json: async () => ({ revision, count: titles.length,
          free: 2000000, healthy: true, tracks: titles.map((title, i) =>
            ({ i, title, sr: 12000, duration: 1000, size: 100 })) }) };
      }
      const parsed = new URL(url, 'http://device');
      posts.push({ url: parsed, body: options.body });
      if (parsed.searchParams.get('r') !== revision) return { ok: false, status: 409 };
      if (parsed.pathname === '/audio/delete') {
        titles.splice(Number(parsed.searchParams.get('i')), 1);
        revision += '-next';
      }
      return { ok: true, status: 200 };
    },
  };
  vm.createContext(sandbox);
  vm.runInContext(source, sandbox);
  await new Promise(resolve => setImmediate(resolve));
  const oldDelete = element('tracks').children[1].children[2].children[1].onclick;
  const oldTitle = element('tracks').children[1].children[2].children[0].onclick;
  sandbox.titleImageBytes = async () => new Uint8Array([1]);
  titles = ['B', 'C'];
  revision = 'session-2';
  await sandbox.load();
  await oldDelete();
  assert.equal(posts.at(-1).url.searchParams.get('r'), 'session-1');
  assert.deepEqual(titles, ['B', 'C']);
  assert.match(element('message').textContent, /曲库已变化/);
  assert.equal(posts.length, 1, 'never retry a destructive request after refresh');
  await oldTitle();
  assert.equal(posts.at(-1).url.searchParams.get('r'), 'session-1');
  assert.match(element('message').textContent, /曲库已变化/);
  assert.equal(posts.length, 2);
  const before = posts.length;
  await sandbox.remove(0, undefined);
  assert.equal(posts.length, before, 'old pages without a revision must only refresh');
  await element('tracks').children[0].children[2].children[1].onclick();
  assert.deepEqual(titles, ['C']);

  // A competing edit after audio upload must not attach its title to another song.
  element('file').files = [{ name: 'New.mp3' }];
  sandbox.encode = async () => new Uint8Array([1]);
  sandbox.postAudio = async () => {
    revision = 'session-4';
    return JSON.stringify({ index: 0, revision: 'session-3' });
  };
  await element('send').onclick();
  assert.equal(posts.at(-1).url.searchParams.get('r'), 'session-3');
  assert.match(element('message').textContent, /音乐已保存，但歌名图失败：.*曲库已变化/);
  assert.deepEqual(titles, ['C']);
  assert.ok(gets >= 5);
  console.log('web catalog: 5 request/version/conflict scenarios passed');
}

main().catch(error => { console.error(error); process.exitCode = 1; });
