package models.account;

import com.mohiva.play.silhouette.impl.providers.CredentialsProvider
import com.mohiva.play.silhouette.impl.providers.oauth2.GoogleProvider
import play.api.mvc.QueryStringBindable
import play.api.libs.json._
import play.api.libs.json.Json._
import scala.Left
import scala.Right
import scala.math.BigDecimal.int2bigDecimal

object AuthProvider extends Enumeration {
  type AuthProvider = Value
  val Unknown = Value(0)
  val Credentials = Value(1)
  val Google = Value(2)
  val Facebook = Value(3)
  val Twitter = Value(4)
  
  val providerMap = Map[String, AuthProvider](
        CredentialsProvider.ID -> Credentials, 
        GoogleProvider.ID -> Google)
  
  def fromProviderId(name: String): Option[AuthProvider] = {
    providerMap.contains(name) match {
      case true => Some(providerMap(name))
      case false => None
    }
  }
  
  implicit val jsonWrite: Writes[AuthProvider] = new Writes[AuthProvider] {
    def writes(s: AuthProvider): JsValue = JsNumber(s.id)
  }
  
  implicit val jsonRead: Reads[AuthProvider] = new Reads[AuthProvider] {
    def reads(in: JsValue): JsResult[AuthProvider] = {
      in match {
        case JsString(s) => {
          try {
            JsSuccess(AuthProvider(s.toInt))
          } catch {
            case _: NoSuchElementException => JsError(s"Cannot map $s to AuthProviderType")
          }
        }
        
        case JsNumber(n) => {
          try {
            JsSuccess(AuthProvider(n.toInt))
          }
          catch {
            case _: NoSuchElementException => JsError(s"Cannot map $n to AuthProviderType")
          }
        }
        
        case _ => JsError("cannot map unknown input to AuthProviderType")
      }
    }
  }  
  
  implicit def bindableAuthProvider = new QueryStringBindable[AuthProvider] {
    def bind(key: String, params: Map[String, Seq[String]]) = {
      params.get(key).flatMap(_.headOption).map { p =>
        AuthProvider(p.toInt) match {
          case s: AuthProvider => Right(s)
          case _ => Left(s"Cannot parse parameter $key as an Enum: AuthProviderType")
        }
      }
    }
    def unbind(key: String, value: AuthProvider) = value.toString
  }
}