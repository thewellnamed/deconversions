package models

import play.api.mvc.QueryStringBindable
import play.api.libs.json._
import play.api.libs.json.Json._
import scala.Left
import scala.Right
import scala.math.BigDecimal.int2bigDecimal

object CodeType extends Enumeration {
  type CodeType = Value
  val None = Value(0)
  val Image = Value(1)
  val Thread = Value(2)
  val Meta = Value(3)
  
  implicit val jsonWrite: Writes[CodeType] = new Writes[CodeType] {
    def writes(s: CodeType): JsValue = JsNumber(s.id)
  }
  
  implicit val jsonRead: Reads[CodeType] = new Reads[CodeType] {
    def reads(in: JsValue): JsResult[CodeType] = {
      in match {
        case JsString(s) => {
          try {
            JsSuccess(CodeType(s.toInt))
          } catch {
            case _: NoSuchElementException => JsError(s"Cannot map $s to AuthProviderType")
          }
        }
        
        case JsNumber(n) => {
          try {
            JsSuccess(CodeType(n.toInt))
          }
          catch {
            case _: NoSuchElementException => JsError(s"Cannot map $n to AuthProviderType")
          }
        }
        
        case _ => JsError("cannot map unknown input to AuthProviderType")
      }
    }
  }  
  
  implicit def bindableAuthProviderType = new QueryStringBindable[CodeType] {
    def bind(key: String, params: Map[String, Seq[String]]) = {
      params.get(key).flatMap(_.headOption).map { p =>
        CodeType(p.toInt) match {
          case s: CodeType => Right(s)
          case _ => Left(s"Cannot parse parameter $key as an Enum: AuthProviderType")
        }
      }
    }
    def unbind(key: String, value: CodeType) = value.toString
  }
}