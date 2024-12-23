package models.account;

import play.api.mvc.QueryStringBindable
import play.api.libs.json._
import play.api.libs.json.Json._
import scala.Left
import scala.Right
import scala.math.BigDecimal.int2bigDecimal

object AuthProviderType extends Enumeration {
  type AuthProviderType = Value
  val Unknown = Value(0)
  val Credentials = Value(1)
  val OAuth1 = Value(2)
  val OAuth2 = Value(3)
  val OpenID = Value(4)
  
  implicit val jsonWrite: Writes[AuthProviderType] = new Writes[AuthProviderType] {
    def writes(s: AuthProviderType): JsValue = JsNumber(s.id)
  }
  
  implicit val jsonRead: Reads[AuthProviderType] = new Reads[AuthProviderType] {
    def reads(in: JsValue): JsResult[AuthProviderType] = {
      in match {
        case JsString(s) => {
          try {
            JsSuccess(AuthProviderType(s.toInt))
          } catch {
            case _: NoSuchElementException => JsError(s"Cannot map $s to AuthProviderType")
          }
        }
        
        case JsNumber(n) => {
          try {
            JsSuccess(AuthProviderType(n.toInt))
          }
          catch {
            case _: NoSuchElementException => JsError(s"Cannot map $n to AuthProviderType")
          }
        }
        
        case _ => JsError("cannot map unknown input to AuthProviderType")
      }
    }
  }  
  
  implicit def bindableAuthProviderType = new QueryStringBindable[AuthProviderType] {
    def bind(key: String, params: Map[String, Seq[String]]) = {
      params.get(key).flatMap(_.headOption).map { p =>
        AuthProviderType(p.toInt) match {
          case s: AuthProviderType => Right(s)
          case _ => Left(s"Cannot parse parameter $key as an Enum: AuthProviderType")
        }
      }
    }
    def unbind(key: String, value: AuthProviderType) = value.toString
  }
}