package models

import java.time._
import play.api.libs.json._
import models.account.Account
import models.CodeType.CodeType
import models.CodeValue.CodeValue

case class Image(id: Int, key: String, filename: String, posted: Instant)

case class ImageMeta(
  id: Int, 
  key: String, 
  title: String, 
  discussionURL: String, 
  thread: Option[JsValue], 
  images: List[Image])

case class Comment(id: Int, imageId: Int, accountId: Int, value: String)
case class Code(id: Int, imageId: Int, accountId: Int, codeType: CodeType, value: CodeValue, seq: Int)
  
object Implicits {
  implicit val jsonWriteImage: Writes[Image] = new Writes[Image] {
    def writes(img: Image): JsValue = Json.obj(
      "key" -> img.key,
      "filename" -> img.filename,
      "posted" -> img.posted
    )
  }
  
  implicit val jsonWriteCodeType: Writes[CodeType] = new Writes[CodeType] {
    def writes(codeType: CodeType): JsValue = JsNumber(codeType.id)
  }
  
  implicit val jsonWriteCodeValue: Writes[CodeValue] = new Writes[CodeValue] {
    def writes(value: CodeValue): JsValue = JsNumber(value.id)
  }
 
  implicit val jsonWriteComments: Writes[Option[Map[Int, String]]] = new Writes[Option[Map[Int, String]]] {
    def writes(comments: Option[Map[Int, String]]): JsValue = {
      comments match {
        case None => Json.obj()
        case Some(c) => Json.toJson(c.map { case (id: Int, v: String) => id.toString -> v } toMap)
      }
    }
  }
  
  implicit val jsonWriteCodes: Writes[Option[Map[Int, Map[CodeType, List[CodeValue]]]]] = new Writes[Option[Map[Int, Map[CodeType, List[CodeValue]]]]] {
    def writes(codes: Option[Map[Int, Map[CodeType, List[CodeValue]]]]): JsValue = {
      codes match {
        case None => Json.obj()
        case Some(c) => {
          Json.toJson(c.map { case (id: Int, map: Map[CodeType, List[CodeValue]]) => id.toString -> map.map {
                case (codeType: CodeType, codes: List[CodeValue]) => codeType.id.toString -> codes } } toMap)
        }
      }
    }
  }
  
  implicit val jsonWriteImageMeta: Writes[ImageMeta] = new Writes[ImageMeta] {
    def writes(meta: ImageMeta): JsValue = Json.obj(
      "id" -> meta.id,
      "key" -> meta.key,
      "title" -> meta.title,
      "url" -> meta.discussionURL,
      "images" -> meta.images
    )
  }
}