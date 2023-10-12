export function transCountTime (time) {
  let hour = Math.floor(time / 3600)
  let min = Math.floor((time % 3600) / 60)
  let sec = time % 60
  hour = (hour < 10 ? '0' : '') + hour
  min = (min < 10 ? '0' : '') + min
  sec = (sec < 10 ? '0' : '') + sec
  // return ['0', '2', '3', '7', '4', '5']
  return (hour + min + sec).split('')
}

export function transClockTime (date = new Date(), hasMoment = true) {
  let hour = date.getHours()
  let min = date.getMinutes()
  let sec = date.getSeconds()
  let bar = hour > 11 ? 'pm' : 'am'
  bar = hasMoment ? bar : ''
  hour = hour > 12 ? hour - 12 : hour
  hour = (hour < 10 ? '0' : '') + hour
  min = (min < 10 ? '0' : '') + min
  sec = (sec < 10 ? '0' : '') + sec
  // return ['0', '1', '1', '7', '4', '1', Math.random()>0.5?'p':'a', 'm']
  return (hour + min + sec + bar).split('')
}

export function getFontWeight (str) {
  str = str.toLowerCase()
  let res = null
  let map = {
    'thin': '100',
    'extralight': '200',
    'extra light': '200',
    'ultralight': '200',
    'ultra light': '200',
    'normal': '400',
    'book': '400',
    'roman': '400',
    'medium': '500',
    'semibold': '600',
    'semi bold': '600',
    'demibold': '600',
    'demi bold': '600',
    'extrabold': '800',
    'extra bold': '800',
    'ultrabold': '800',
    'ultra bold': '800',
    'black': '900',
    'heavy': '900'
  }
  let map2 = {
    'bold': '700',
    'light': '300',
    'regular': '400'
  }
  for (let key in map) {
    str.indexOf(key) !== -1 && (res = map[key])
  }
  if (res === null) {
    for (let key in map2) {
      str.indexOf(key) !== -1 && (res = map2[key])
    }
  }
  return res
}
