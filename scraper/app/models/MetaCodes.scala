package models

import play.api.mvc.QueryStringBindable
import play.api.libs.json._
import play.api.libs.json.Json._
import scala.Left
import scala.Right
import scala.math.BigDecimal.int2bigDecimal

object MetaCodeValue extends Enumeration {
  type MetaCodeValue = Value
  val None = Value(0)
  val DenialOfInequality = Value(1)
  val MenAsVictims = Value(2)
  val AntiFeminism = Value(3)
  val ProMen = Value(4)
  
  implicit val jsonWrite: Writes[MetaCodeValue] = new Writes[MetaCodeValue] {
    def writes(s: MetaCodeValue): JsValue = JsNumber(s.id)
  }
  
  implicit val jsonRead: Reads[MetaCodeValue] = new Reads[MetaCodeValue] {
    def reads(in: JsValue): JsResult[MetaCodeValue] = {
      in match {
        case JsString(s) => {
          try {
            JsSuccess(MetaCodeValue(s.toInt))
          } catch {
            case _: NoSuchElementException => JsError(s"Cannot map $s to AuthProviderType")
          }
        }
        
        case JsNumber(n) => {
          try {
            JsSuccess(MetaCodeValue(n.toInt))
          }
          catch {
            case _: NoSuchElementException => JsError(s"Cannot map $n to AuthProviderType")
          }
        }
        
        case _ => JsError("cannot map unknown input to AuthProviderType")
      }
    }
  }  
  
  implicit def bindableAuthProviderType = new QueryStringBindable[MetaCodeValue] {
    def bind(key: String, params: Map[String, Seq[String]]) = {
      params.get(key).flatMap(_.headOption).map { p =>
        MetaCodeValue(p.toInt) match {
          case s: MetaCodeValue => Right(s)
          case _ => Left(s"Cannot parse parameter $key as an Enum: AuthProviderType")
        }
      }
    }
    def unbind(key: String, value: MetaCodeValue) = value.toString
  }
}