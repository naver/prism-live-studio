import { register, format } from 'timeago.js'

const en = require(`@/i18n/en.json`)
const ko = require(`@/i18n/ko.json`)
const id = require(`@/i18n/id.json`)
const pt = require(`@/i18n/pt.json`)
const ja = require(`@/i18n/ja.json`)
const defaultLang = [
  ['just now', 'just now'],
  ['%s seconds ago', 'just now'],
  ['1 minute ago', 'just now'],
  ['%s minutes ago', 'just now'],
  ['1 hour ago', 'just now'],
  ['%s hours ago', 'just now'],
  ['1 day ago', 'just now'],
  ['%s days ago', 'just now'],
  ['1 week ago', 'just now'],
  ['%s weeks ago', 'just now'],
  ['1 month ago', 'just now'],
  ['%s months ago', 'just now'],
  ['1 year ago', 'just now'],
  ['%s years ago', 'just now']
]
register('en', (diff, index) => {
  return (en.timeago || defaultLang)[index]
})

register('ko', (diff, index) => {
  return (ko.timeago || defaultLang)[index]
})

register('id', (diff, index) => {
  return (id.timeago || defaultLang)[index]
})

register('pt', (diff, index) => {
  return (pt.timeago || defaultLang)[index]
})

register('ja', (diff, index) => {
  return (ja.timeago || defaultLang)[index]
})



export default format
