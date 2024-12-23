package utils

import play.api.db._
import play.api.libs.json._
import anorm._
import anorm.SqlParser._
import anorm.NamedParameter.symbol
import scala.Left
import scala.Right
import java.time.Instant

import models.account.Account

object Implicits {
  // Map between PostgreSQL jsonb type and native json
  implicit def jsonToString: Column[JsValue] = Column.nonNull1{ (value, meta) =>
    val MetaDataItem(qualified, nullable, clazz) = meta
    value match {
      case pgo: org.postgresql.util.PGobject => Right(Json.parse(pgo.getValue))
      case _ => Left(TypeDoesNotMatch("Cannot convert " + value + ":" +
          value.asInstanceOf[AnyRef].getClass + " to JsValue for column " + qualified))
    }
  }  
  
  implicit val jsonWriteAccount: Writes[Account] = new Writes[Account] {
    def writes(acct: Account): JsValue = Json.obj(
      "id"    -> acct.id,
      "email" -> acct.email,
      "name"  -> acct.name,
      "perms" -> acct.perms
    )
  }
}