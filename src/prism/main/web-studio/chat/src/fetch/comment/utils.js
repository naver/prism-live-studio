import queryString from 'querystring'
import Cookies from 'js-cookie'

export function urlParse (url) {
  let temp = !~url.indexOf('?') && !~url.indexOf('=') ? '' : url.split('?').pop()
  return queryString.parse(temp)
}

export function urlStringify (url, data) {
  let params = urlParse(url)
  let params2 = queryString.stringify({...params, ...data})
  return (url.split('?').shift() || '') + (params2 ? '?' + params2 : '')
}

export function getCookie (key) {
  window.setCookie = setCookie
  return Cookies.get(key)
}

export function setCookie (key, value, option = {}) {
  window.setCookie = setCookie
  return Cookies.set(key, value, option)
}

export function removeCookie (key) {
  return Cookies.remove(key)
}
