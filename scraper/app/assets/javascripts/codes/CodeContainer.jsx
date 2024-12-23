var CodeContainer = React.createClass({
	getInitialState: function() {
		return { active: this.props.view, codes: this.props.codes };
	},
	
	componentDidMount: function() {
		$('.code-container a[data-toggle="tab"]').on('shown.bs.tab', function (e) {
			  var target = $(e.target).attr("href").substring(1);
			  this.setState({ active:  target });
			  this.props.onSelectCodeTab(target);
		}.bind(this));
	},
	
	componentWillUnmount: function() {
		$('.code-container a[data-toggle="tab"]').off('shown.bs.tab');
	},
	
	componentWillReceiveProps: function(newProps) {
		this.setState({ codes: newProps.codes });
	},
	
	handleCodeUpdate: function(imageId, data) {
		
		var codes = this.state.codes;
		codes[this.props.account.id] = data;
		
		this.props.onUpdate(imageId, "codes", data, true);
		this.setState({ codes: codes });
	},
	
	render: function() {
		var mayEdit = AccountPermission.check(this.props.account, AccountPermission.Edit);
		var editorTab = null, editor = null, viewTabClass = "", viewPaneClass = "tab-pane";
		
		if (mayEdit) {
			var accountId = this.props.account ? this.props.account.id : 0;
			var codes = (typeof(this.state.codes[accountId]) !== "undefined") ? this.state.codes[accountId]: {};
			var editTabClass = "", editPaneClass = "tab-pane", viewPaneClass = "tab-pane";
			
			if (this.state.active == 'editor') {
				editTabClass = "active";
				editPaneClass += " active";
			} 
			
			editorTab = (<li className={editTabClass}><a href="#editor" data-toggle="tab">My Codes</a></li>);
			editor = (					
				<div className={editPaneClass} id="editor">
					<CodeEditor 
						session={this.props.session}
						account={this.props.account}
						meta={this.props.meta} 
						codes={codes} 
						csrfToken={this.props.csrfToken} 
						onUpdate={this.handleCodeUpdate} />
				</div>
			);
		} 
		
		if (this.state.active == 'viewer') {
			viewTabClass = "active";
			viewPaneClass += " active";
		}
		
		return (
			<div className="code-container">
				<ul className="nav nav-tabs">
					{editorTab}
					<li className={viewTabClass}><a href="#viewer" data-toggle="tab">All Codes</a></li>
				</ul>
				<div className="tab-content">
					{editor}
					<div className={viewPaneClass} id="viewer">
						<CodeViewer codes={this.props.codes} />
					</div>
				</div>
			</div>	
		);
	}
});