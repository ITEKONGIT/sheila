const tabs = [...document.querySelectorAll('.platform-tab')];
const releaseCards = [...document.querySelectorAll('[data-release]')];
const releaseVersion = document.querySelector('#release-version');
const releaseStatus = document.querySelector('#release-status');
const downloadLinks = {
  'windows-x64': document.querySelector('#windows-download'),
  'linux-x64': document.querySelector('#linux-download'),
};

const releaseBaseUrl = String(window.SSHEILA_RELEASE_BASE_URL || '').replace(/\/$/, '');
const manifestUrl = releaseBaseUrl ? `${releaseBaseUrl}/latest.json` : '/latest.json';

function formatDate(value) {
  if (!value) return 'Latest build';
  const date = new Date(value);
  return Number.isNaN(date.getTime()) ? 'Latest build' : new Intl.DateTimeFormat(undefined, {day: '2-digit', month: 'short', year: 'numeric'}).format(date).toUpperCase();
}

function setDownload(platform, artifact) {
  const link = downloadLinks[platform];
  if (!link) return;
  const available = Boolean(artifact?.url);
  link.href = available ? artifact.url : '#';
  link.textContent = available ? `Download ${artifact.name || 'build'} ↓` : 'Not available';
  link.classList.toggle('is-disabled', !available);
  link.setAttribute('aria-disabled', String(!available));
  if (available) link.setAttribute('download', '');
  else link.removeAttribute('download');
  const detail = document.querySelector(`#${platform === 'windows-x64' ? 'windows' : 'linux'}-detail`);
  if (detail) detail.textContent = available ? `${artifact.name || 'Latest build'} · ${artifact.size_bytes ? `${Math.round(artifact.size_bytes / 1024 / 1024)} MB` : 'ready to download'}` : 'No build is listed for this platform yet.';
}

async function loadReleaseManifest() {
  try {
    const response = await fetch(manifestUrl, {headers: {accept: 'application/json'}});
    if (!response.ok) throw new Error(`Manifest request failed (${response.status})`);
    const manifest = await response.json();
    releaseVersion.textContent = manifest.version || manifest.tag || 'Latest build';
    document.querySelector('#release-status').innerHTML = `<span class="status-dot"></span> Published ${formatDate(manifest.published_at)} · <a href="${manifest.github_release_url || '#'}" target="_blank" rel="noreferrer">release notes ↗</a>`;
    setDownload('windows-x64', manifest.platforms?.['windows-x64']);
    setDownload('linux-x64', manifest.platforms?.['linux-x64']);
    const sourceLink = document.querySelector('#source-link');
    if (sourceLink && manifest.github_release_url) sourceLink.href = manifest.github_release_url;
  } catch (error) {
    releaseVersion.textContent = 'Unavailable';
    releaseStatus.innerHTML = '<span class="status-dot"></span> Release manifest is not configured yet.';
    setDownload('windows-x64', null);
    setDownload('linux-x64', null);
    console.warn(error);
  }
}

function setPlatform(platform) {
  tabs.forEach((tab) => {
    const active = tab.dataset.platform === platform;
    tab.classList.toggle('is-active', active);
    tab.setAttribute('aria-selected', String(active));
  });
  releaseCards.forEach((card) => card.classList.toggle('is-hidden', card.dataset.release !== platform));
}

tabs.forEach((tab) => tab.addEventListener('click', () => setPlatform(tab.dataset.platform)));
loadReleaseManifest();
