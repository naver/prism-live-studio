export const TWITCH_EMOTICON_URL = 'https://static-cdn.jtvnw.net/emoticons/v1/{code}/3.0'

const TWITCH_BADGES_LIBRARY_URL = 'https://badges.twitch.tv/v1/badges/global/display'
const TWITCH_CHANNEL_BADGES_LIBRARY_URL = 'https://badges.twitch.tv/v1/badges/channels/{channelId}/display'

const TWITCH_BITS_LIBRARY_URL = 'https://api.twitch.tv/v5/bits/actions'

let status = false

export let TWITCH_BADGES_LIBRARY = {
  total: null,
  channel: null
}

export let TWITCH_BITS_LIBRARY = {
  bits: null,
  regexp: null,
  amounts: null
}

export async function getTwitchResources({channelId, clientId}) {
  if (
    (TWITCH_BADGES_LIBRARY.total &&
    TWITCH_BADGES_LIBRARY.channel &&
    TWITCH_BITS_LIBRARY.bits) ||
    !channelId ||
    status) return
  status = true

  await getBadgesTotalLibrary()
  await getBadgesChannelLibrary(channelId)
  await getBitsLibrary(channelId, clientId)

  status = false
}

async function getBitsLibrary(channelId, clientId) {
  if (TWITCH_BITS_LIBRARY.bits || !clientId) return
  const res = await fetch(`${TWITCH_BITS_LIBRARY_URL}?channel_id=${channelId}`, {
    headers: {
      'Client-ID': clientId,
      Accept: 'application/vnd.twitchtv.v5+json'
    }
  })
  const data = await res.json()

  const actions = data && handleBitsData(data.actions)
  if (actions) {
    TWITCH_BITS_LIBRARY.bits = actions
    TWITCH_BITS_LIBRARY.regexp = new RegExp('(?<=^|\\s)(' + Object.keys(actions).join('|') + ')(\\d+)(?=\\s|$)', 'g')
    TWITCH_BITS_LIBRARY.amounts = Object.keys(Object.values(actions)[0]).map(el => Number(el))
  }
}

async function getBadgesTotalLibrary() {
  if (TWITCH_BADGES_LIBRARY.total) return
  const res = await fetch(TWITCH_BADGES_LIBRARY_URL)
  const data = await res.json()

  TWITCH_BADGES_LIBRARY.total = data && data.badge_sets
}

async function getBadgesChannelLibrary(channelId) {
  if (TWITCH_BADGES_LIBRARY.channel) return

  const res = await fetch(TWITCH_CHANNEL_BADGES_LIBRARY_URL.replace('{channelId}', channelId))
  const data = await res.json()

  TWITCH_BADGES_LIBRARY.channel = data && data.badge_sets
}

function handleBitsData(data) {
  let res = {}
  data.forEach(el => {
    let detail = res[el.prefix] = {}
    el.tiers.forEach(level => {
      detail[level.min_bits] = {
        color: level.color,
        image: level.images.light.animated[3]
      }
    })
  })
  return res
}
