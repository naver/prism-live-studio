import { CHANNEL } from '@/assets/js/constants'
import { handleJson } from '@/assets/js/utils'
import {
  AFREECATV_EMOTICON_REGEX,
  AFREECATV_EMOTICON,
  AFREECATV_SUBSCRIPTION_EMOTICON,
  AFREECATV_EMOTICON_URL
} from '@/assets/js/afreecatvResources'

export default {
  methods: {
    isAfreecatvOwner(userId, chatSendId) {
      if (!userId || !chatSendId) return
      const regexp = new RegExp(/\([\s\S]*\)/g)

      return userId.replace(regexp, '') === chatSendId.replace(regexp, '')
    },
    handleAfreecatvChat(data, userId) {
      if (data.livePlatform !== CHANNEL.AFREECATV) return
      if (typeof data.rawMessage === 'string') {
        data.rawMessage = handleJson(data.rawMessage)
      }
      const message = data.rawMessage.message
      if (message) {
        data._isSelf = data.author.isChatOwner && this.isAfreecatvOwner(userId, message.id)
        data._isManager = data.author.isChatOwner && !data._isSelf
        data._badges = Object.values(message.badgeMap || {})
        data._displayName = `${data.author.displayName}(${message.id})`
        data._message = data.message && transEmotion(data.message)
        return data
      } else {
        const rawMessage = data.rawMessage
        const key = Object.keys(rawMessage)[0]
        const value = key && rawMessage[key]
        const aText = this.$i18n.afreecaTV
        let notice
        switch (key) {
          case 'starBalloon':
          case 'sticker':
            notice = aText[key].replace('%s', value.sendNickName).replace('%d', value.cnt)
            break
          case 'tempBanChat':
            notice = aText[key].replace('%s', value.banedId).replace('%d', value.count).replace('%t', value.count > 1 ? 's' : '')
            break
          case 'participant':
            notice = aText[key][value.enterState.toLocaleLowerCase()].replace('%s', value.nickName)
            break
          case 'cancelForcedWithdrawl':
            notice = aText[key].replace('%s', value.nickName)
            break
          case 'freezeOrMeltChat':
            notice = value.isFreeze ? aText.freezeOrMeltChat : aText.cancelFreezeOrMeltChat
            break
          default:
            console.error('Can not find the specified notice type: ', key)
            break
        }

        if (notice) {
          data._notice = notice
          return data
        }
      }
    }
  }
}

function handlePicture(match, p) {
  const platform = 'mobile'
  const data = AFREECATV_EMOTICON[match] || AFREECATV_SUBSCRIPTION_EMOTICON[match]
  if (data) {
    const src = AFREECATV_EMOTICON_URL[platform] + data[platform] + '.png'
    return `<img class="afreecatv-emoticon" src="${src}" alt="${p}"/>`
  }
}

function transEmotion(message) {
  return message.replace(AFREECATV_EMOTICON_REGEX, (match, p) => {
    return handlePicture(match, p)
  })
}
