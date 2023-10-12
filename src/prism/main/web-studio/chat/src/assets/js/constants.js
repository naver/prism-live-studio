export const CHANNEL = {
  TWITCH: 'TWITCH',
  YOUTUBE: 'YOUTUBE',
  FACEBOOK: 'FACEBOOK',
  NAVERTV: 'NAVERTV',
  VLIVE: 'VLIVE',
  AFREECATV: 'AFREECATV',
  SHOPPINGLIVE: 'SHOPPINGLIVE'
}

export const CHANNEL_ON_ALL_PAGE = [CHANNEL.YOUTUBE, CHANNEL.NAVERTV, CHANNEL.TWITCH, CHANNEL.FACEBOOK, CHANNEL.AFREECATV]

export const CHAT_CAN_DELETED_PLATFORMS = [CHANNEL.SHOPPINGLIVE, CHANNEL.YOUTUBE, CHANNEL.NAVERTV, CHANNEL.VLIVE]

export const CHANNEL_NAME = {
  NAVERTV: 'NAVER TV',
  VLIVE: 'V LIVE',
  AFREECATV: 'afreecaTV',
  FACEBOOK: 'Facebook',
  SHOPPINGLIVE: 'NAVER Shopping LIVE',
}

export const MAX_INPUT_CHARACTER = 65
export const MAX_INPUT_CHARACTER_NAVERTV = 65
export const MAX_INPUT_CHARACTER_VLIVE = 60
export const MAX_INPUT_CHARACTER_AFREECATV = 128

export const DEFAULT_LANG = 'ko'

export const VLIVE_CHAT_IMAGE_BASE_URL = 'http://beta.v.phinf.naver.net/'
export const VLIVE_CHAT_IMAGE_BASE_URL_PRO = 'https://v-phinf.pstatic.net/'

export const MAX_CACHED_CHAT_COUNT = 2000
export const MAX_LOCAL_CACHED_CHAT_COUNT = 200 //崩溃最多恢复本地缓存200条聊天信息

export const AFREECATV_EMOTICON_REGEX = new RegExp(/\/(샤방|윽|휘파람|짜증|헉|하이|개좋아|개도발|개털림|개감상|개화나|개이득|개번쩍|짱좋아|피식|헐|감상중|화나|하하|ㅠㅠ|주작|꿀잼|업|갑|묻|심쿵|스겜|추천|인정|사이다|더럽|극혐|매너챗|강퇴|드루와|아잉|기겁|우울|쳇|ㅋㅋ|졸려|최고|엉엉|후훗|부끄|제발|덜덜|좋아|반함|멘붕|버럭|우엑|뽀뽀|심각|쥘쥘|헤헤|훌쩍|코피|철컹철컹|섬뜩|꺄|굿|글썽|황당|정색|피곤|사랑|좌절|사탕|RIP|건빵|사과|귤|겁나좋군|근육녀|근육남|박수|소주|짱|꽃|왕|썰렁|무지개|태극기|절교|하트|불|별|폭탄|폭죽|보석|금|돈|맥주|입술|콜!|번쩍|19|즐거워|케이크|약|SK|두산|LG|롯데|삼성|한화|기아|키움|NC|KT|SK마|두산마|LG마|롯데마|삼성마|한화마|기아마|키움마|NC마|KT마|메가폰|신문|봉투|아프리카|SKT|DRX|한화생명|젠지|케이티|담원|샌드박스|설해원|다이나믹스|확인요|미션|ㅇㅋ|티키타카|ㄱㄴㅇ|동의|굿밤|맴찢|나이따|ㄱㄱ|조오치|ㄴㅇㅂㅈ|데헷|런|각|실화|ㅇㅈ|ㅇㄱㄹㅇ|반사|TMI|JMT|할많하않|현타|엄근진|머쓱|탈룰라|누나|탈주|손절|하락)\//g)
