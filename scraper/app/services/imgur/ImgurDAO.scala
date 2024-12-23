package services.imgur

import anorm._
import anorm.SqlParser._
import play.api.db._
import play.api.Logger
import play.api.Environment
import play.api.libs.concurrent.Execution.Implicits.defaultContext
import play.api.libs.json._
import scala.concurrent.duration._
import scala.concurrent.Future
import javax.inject._
import java.time.Instant
import java.nio.file.Files
import java.nio.file.Path
import java.nio.file.Paths
import models._
import models.CodeType.CodeType
import models.CodeValue.CodeValue
import models.account.Account

class ImgurDAO @Inject()(db: Database, env: Environment)
{
  val imageMetaParser = {
    get[Int]("image_id") ~
    get[String]("imgur_key") ~
    get[String]("title") ~
    get[String]("reddit_url") map {
      case id ~ key ~ title ~ url => ImageMeta(id, key, title, url, None, List.empty[Image])
    }
  }  
  
  def imageParser = {
    get[Int]("image_id") ~
    get[String]("imgur_key") ~
    get[String]("filename") ~
    get[Instant]("posted") map {
      case id ~ key ~ filename ~ posted => Image(id, key, filename, posted)
    }
  }
  
  def commentParser = {
    get[Int]("comment_id") ~
    get[Int]("image_id") ~
    get[Int]("account_id") ~
    get[String]("comment") map {
      case id ~ imageId ~ accountId ~ comment => Comment(id, imageId, accountId, comment)
    }
  }
  
  def codeParser = {
    get[Int]("code_id") ~
    get[Int]("image_id") ~
    get[Int]("account_id") ~
    get[Int]("type") ~
    get[Int]("value") ~
    get[Int]("seq") map {
      case id ~ imageId ~ accountId ~ typeId ~ value ~ seq => Code(id, imageId, accountId, CodeType(typeId), CodeValue(value), seq)
    }
  }
  
  def fetchAll(): Future[List[ImageMeta]] = Future {
    db.withConnection { implicit c =>
      val meta = SQL("SELECT image_id, imgur_key, title, reddit_url FROM images_meta ORDER BY image_id").as(imageMetaParser.*)
      val imageDict  = SQL("SELECT image_id, imgur_key, filename, posted FROM images").as(imageParser.*).groupBy(_.id)
      
      meta.map { m => ImageMeta(m.id, m.key, m.title, m.discussionURL, None, imageDict(m.id)) } 
    }
  }
  
  def fetchWithCodes(metaCodes: List[Int], imageCodes: List[Int]): Future[List[ImageMeta]] = {
    if (metaCodes.isEmpty && imageCodes.isEmpty) {
      fetchAll()
    } else {
      Future {
        db.withConnection { implicit c =>
          val query = new StringBuilder("SELECT DISTINCT m.image_id, m.imgur_key, m.title, m.reddit_url FROM images_meta m ")
          val where = new StringBuilder("")
          val haveMetaCodes = (!metaCodes.isEmpty && (metaCodes.length > 1 || metaCodes(0) != 0))
          val haveMetaUncoded = metaCodes.contains(0)
          val haveImageCodes = (!imageCodes.isEmpty && (imageCodes.length > 1 || imageCodes(0) != 0))
          val haveImageUncoded = imageCodes.contains(0)
          
          if (haveMetaCodes) {
            query.append("LEFT JOIN codes mc ON (mc.type={metaType} AND mc.image_id=m.image_id) ")
            
            if (haveMetaUncoded) {
              where.append("""(mc.value IN ({metaFilters}) OR NOT EXISTS 
                                 (SELECT mc2.code_id FROM codes mc2 WHERE mc2.type={metaType} AND mc2.image_id=m.image_id))""")
            } else {
              where.append("mc.value IN ({metaFilters})")
            }
          } else if (haveMetaUncoded) {
            where.append("NOT EXISTS (SELECT mc2.code_id FROM codes mc2 WHERE mc2.type={metaType} AND mc2.image_id=m.image_id)")
          }
          
          if (haveImageCodes) {
            query.append("LEFT JOIN codes c ON (c.type={imageType} AND c.image_id=m.image_id) ")
            if (haveMetaCodes || haveMetaUncoded) {
              where.append(" AND ")
            }
            
            if (haveImageUncoded) {
              where.append("""(c.value IN ({imageFilters}) OR NOT EXISTS 
                                 (SELECT c2.code_id FROM codes c2 WHERE c2.type={imageType} AND c2.image_id=m.image_id))""")
            } else {
              where.append("c.value IN ({imageFilters})")
            }
          } else if (haveImageUncoded) {
            if (haveMetaCodes || haveMetaUncoded) {
              where.append(" AND ")
            }
            where.append("NOT EXISTS (SELECT c2.code_id FROM codes c2 WHERE c2.type={imageType} AND c2.image_id=m.image_id)")
          }
          
          query.append("WHERE (")
          query.append(where)
          query.append(") ORDER BY m.image_id")
          
          val meta = SQL(query.toString)
              .on('metaType -> CodeType.Meta.id, 
                  'metaFilters -> metaCodes,
                  'imageType -> CodeType.Image.id, 
                  'imageFilters -> imageCodes)
              .as(imageMetaParser.*)
   
          val imageDict  = SQL("SELECT image_id, imgur_key, filename, posted FROM images").as(imageParser.*).groupBy(_.id)
          
          meta.map { m => ImageMeta(m.id, m.key, m.title, m.discussionURL, None, imageDict(m.id)) } 
        }
      }
    }
  }
  
  def fetchCodes(imageId: Int, accountId: Int): Future[Option[Map[Int, Map[CodeType, List[CodeValue]]]]] = Future {
    db.withConnection { implicit c =>
      val codes = SQL("SELECT code_id, image_id, account_id, type, value, seq FROM codes WHERE image_id={imageId} ORDER BY seq")
          .on('imageId -> imageId)
          .as(codeParser.*)
      
      codes.length match {
        case 0 => None
        case _ => Some(codes.groupBy(_.accountId).mapValues { codes => codes.groupBy(_.codeType).mapValues { c => c.map(_.value) } })
      }
    }
  }
  
  def fetchComments(imageId: Int): Future[Option[Map[Int, String]]] = Future {
    db.withConnection { implicit c =>
      val comments = SQL("SELECT comment_id, image_id, account_id, comment FROM comments WHERE image_id={imageId}")
          .on('imageId -> imageId)
          .as(commentParser.*)
      
      comments.length match {
        case 0 => None
        case _ => Some(comments.map { comment => comment.accountId -> comment.value } toMap)
      }
    }
  }
  
  def fetchThread(imageId: Int): Future[JsValue] = Future {
    db.withConnection { implicit c =>
      val thread = SQL("SELECT reddit_thread FROM images_meta WHERE image_id={imageId}")
          .on('imageId -> imageId)
          .as(SqlParser.str("reddit_thread").singleOpt)
          
      thread match {
        case Some(t) => Json.parse(t)
        case None => Json.obj()
      }
    }
  }
  
  def addMeta(meta: ImageMeta) = {
    Logger.info("insert meta, key=" + meta.key)
    
    db.withConnection { implicit c =>
      val existing = SQL("SELECT count(*) as items FROM images_meta WHERE imgur_key={key}")
            .on('key -> meta.key)
            .as(SqlParser.int("items").single)

      if (existing == 0) {
        val id = SQL("""INSERT INTO images_meta (imgur_key, title, reddit_url, reddit_thread) VALUES ({key}, {title}, {url}, {thread})""")
            .on('key -> meta.key,
                'title -> meta.title,
                'url -> meta.discussionURL,
                'thread -> meta.thread.getOrElse(Json.obj()).toString())
            .executeInsert()

        if (id.isEmpty) {
          Logger.error("insert into images_meta failed");
        }
        
        id.get
      }
      else {
        Logger.warn("found existing entry for key=" + meta.key)
        0
      }
    }
  }
  
  def addImage(imageId: Long, img: Image, data: Array[Byte]) = {  
    Logger.info("addImage: id=" + imageId + ", img=" + img.filename)
    
    val checksum = play.api.libs.Codecs.md5(data)
    
    db.withConnection { implicit c =>
      val existing = SQL("SELECT count(*) as items FROM images WHERE checksum={checksum}::uuid")
          .on('checksum -> checksum)
          .as(SqlParser.int("items").single)
          
      if (existing == 0) {
        val path = Paths.get(env.rootPath + "/imgur/" + img.filename)
        Files.write(path, data)        
        
        val id = SQL("""INSERT INTO images (image_id, checksum, imgur_key, filename, posted) 
               VALUES ({id}, {checksum}::uuid, {key}, {filename}, {posted})""")
           .on('id -> imageId,
               'checksum -> checksum,
               'key -> img.key,
               'filename -> img.filename,
               'posted -> img.posted)
           .executeInsert()
           
        if (id.isEmpty) {
          Logger.error("insert into images failed")
        }
      }
    }
  }
  
  def updateCode(code: Code) = Future {
    db.withConnection { implicit c =>
      val existing = SQL("SELECT code_id FROM codes WHERE account_id={accountId} AND image_id={imageId} AND type={type} AND seq={seq}")
          .on('accountId -> code.accountId,
              'imageId -> code.imageId,
              'type -> code.codeType.id,
              'seq -> code.seq)
          .as(SqlParser.int("code_id").singleOpt)
      
      val result = existing match {
        case Some(id) => {
          SQL("UPDATE codes SET value={value}, last_updated={lastUpdated} WHERE code_id={id}")
            .on('value -> code.value.id, 
                'lastUpdated -> Instant.now,
                'id -> existing)
            .executeUpdate()
        }

        case None => {
          SQL("INSERT INTO codes (image_id, account_id, type, seq, value, last_updated) VALUES ({imageId}, {accountId}, {type}, {seq}, {value}, {lastUpdated})")
              .on('imageId -> code.imageId,
                  'accountId -> code.accountId,
                  'type -> code.codeType.id,
                  'seq -> code.seq,
                  'value -> code.value.id,
                  'lastUpdated -> Instant.now)
              .executeInsert(SqlParser.scalar[Int].single)
        } 
      }
      
      result > 0
    }
  }
  
  def deleteCode(code: Code) = Future {
    db.withConnection { implicit c =>
      SQL("DELETE FROM codes WHERE account_id={accountId} AND image_id={imageId} AND type={type} AND seq={seq}")
        .on('accountId -> code.accountId,
            'imageId -> code.imageId,
            'type -> code.codeType.id,
            'seq -> code.seq)
        .executeUpdate()
      
      SQL("UPDATE codes SET seq=seq-1 WHERE account_id={accountId} AND image_id={imageId} AND type={type} AND seq > {seq}")
        .on('accountId -> code.accountId,
            'imageId -> code.imageId,
            'type -> code.codeType.id,
            'seq -> code.seq)
        .executeUpdate()
      true
    }
  }
  
  def updateComment(comment: Comment) = Future {
    db.withConnection { implicit c =>
      val existing = SQL("SELECT comment_id FROM comments WHERE account_id={accountId} AND image_id={imageId}")
          .on('accountId -> comment.accountId,
              'imageId -> comment.imageId)
          .as(SqlParser.int("comment_id").singleOpt)
      
      val result = existing match {
        case Some(id) => {
          SQL("UPDATE comments SET comment={value} WHERE comment_id={id}")
            .on('value -> comment.value, 'id -> existing)
            .executeUpdate()
        }

        case None => {
          SQL("INSERT INTO comments (image_id, account_id, comment) VALUES ({imageId}, {accountId}, {value})")
              .on('imageId -> comment.imageId,
                  'accountId -> comment.accountId,
                  'value -> comment.value)
              .executeInsert(SqlParser.scalar[Int].single)
        } 
      }
      
      result > 0
    }
  }
  
  def deleteComment(comment: Comment) = Future {
    db.withConnection { implicit c =>
      SQL("DELETE FROM comments WHERE account_id={accountId} AND image_id={imageId}")
        .on('accountId -> comment.accountId,
            'imageId -> comment.imageId)
        .executeUpdate()
        
      true
    }
  }
}
