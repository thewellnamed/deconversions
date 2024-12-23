package models.account

import play.api.data.Form
import play.api.data.Forms._

/**
 * The form which handles the sign up process.
 */
object LoginForm {
  val form = Form(
    mapping(
      "email"     -> nonEmptyText,
      "password"  -> nonEmptyText
    )(Data.apply)(Data.unapply)
  )
  
  case class Data(
    email: String,
    password: String)
}