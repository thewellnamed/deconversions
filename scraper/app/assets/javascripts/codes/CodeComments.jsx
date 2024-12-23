var CodeComments = React.createClass({
	getInitialState: function() {
		return { meta: this.props.meta, accounts: this.props.accounts, account: this.props.account, data: this.props.comments };
	},
	
	componentWillReceiveProps: function(newProps) {
		this.setState({ meta: newProps.meta, data: newProps.comments, accounts: newProps.accounts, account: newProps.account });
	},
	
	handleEdit: function(comment) {
		var form = {
			imageId: this.state.meta.id,
			comment: comment
		};
		form[this.props.csrfToken.name] = this.props.csrfToken.value;
		
        $.ajax({
            type: 'POST',
            url: '/image/comment',
            data: form,
            dataType: 'json',
            cache: false,
            beforeSend : function(xhr) {
            	xhr.setRequestHeader("X-Auth-Token", this.props.session);
            }.bind(this),
            success: function(results) {
                if (results.success) {
                	var data = this.state.data;
                	data[this.state.account.id] = comment;	
                	this.setState({ data: data });
                	this.props.onUpdate(this.state.meta.id, "comments", data);
                } else {
                	alert('comment update failed');
                }
            }.bind(this),
            error: function() {
            	alert('comment update failed');
            }.bind(this)
        });
	},
	
	render: function() {		
		if (this.state.loading) {
			return (
			    <div className="coder-comments"></div>
			);			
		} else {	
			var imageId = this.state.meta.id;
			var accounts = this.state.accounts;
			var comments  = this.state.data;
			var account = this.state.account;
			var handleEdit = this.handleEdit;
			var mayEdit = AccountPermission.check(account, AccountPermission.Edit);
			
			if (mayEdit && typeof(comments[account.id]) === "undefined") {
				comments[account.id] = "";
			}			
			
			return (
				<div className="coder-comments">
				{Object.keys(comments).map(function(id) {
					var acct = accounts[id];
					var enableEdit = (id == account.id && mayEdit);
					var initEdit = (enableEdit && comments[id] === "");
					return (<CodeComment imageId={imageId} name={acct} enableEdit={enableEdit} initEdit={initEdit} onEdit={handleEdit} value={comments[id]} />);
				})}
				</div>
			);
		}
	}
});

var CodeComment = React.createClass({
	_newPropsForEdit: false,
	
	getInitialState: function() {
		return { 
			imageId: this.props.imageId, 
			name: this.props.name, 
			value: this.props.value, 
			editable: this.props.enableEdit, 
			editing: this.props.initEdit, 
			editValue: "" 
		};
	},
	
	componentWillReceiveProps: function(newProps) {
		
		var initEdit = newProps.initEdit && !this._newPropsForEdit;
		this._newPropsForEdit = false;
		
		this.setState({ 
			imageId: newProps.imageId, 
			name: newProps.name, 
			value: newProps.value, 
			editable: newProps.enableEdit, 
			editing: initEdit,
			editValue: ""					
		});
	},
	
	beginEdit: function(e) {
		this.setState({ editing: true, editValue: this.state.value });
		e.preventDefault();
	},
	
	endEdit: function(e) {
		this._newPropsForEdit = true;
		this.props.onEdit(this.state.editValue);
		this.setState({ editing: false, editValue: "" });		
		e.preventDefault();
	},
	
	cancelEdit: function(e) {
		this.setState({ editing: false, editValue: "" });
		e.preventDefault();
	},
	
	handleTextChange: function(e) {
		this.setState({ editValue: e.target.value });
		e.preventDefault();
	},
	
	render: function() {
		var name = this.state.name;
		if (this.state.editable) {
			if (this.state.editing) {
				name = (<span>{this.state.name} <a href="#" onClick={this.endEdit}><span className="glyphicon glyphicon-ok"></span></a> <a href="#" onClick={this.cancelEdit}><span className="glyphicon glyphicon-remove"></span></a></span>);
			} else {
				name = (<span>{this.state.name} <a href="#" onClick={this.beginEdit}><span className="glyphicon glyphicon-pencil"></span></a></span>);
			}
		}
		
		var comment = this.state.value;
		if (this.state.editing) {
			comment = (<textarea className="comment-edit" onChange={this.handleTextChange} value={this.state.editValue} />);
		}
		
		return (
			<div className="coder-comment">
				<div className="coder-name">{name}</div>
				<div className="comment">{comment}</div>
			</div>				
		);
	}
});