var Viewer = React.createClass({
	ignoreHistoryStateChange: false,
	csrfToken: { name: '', value: '' },
	cache: {},
	
	getInitialState: function() {
		var filters = [];
		filters[CodeType.Meta] = [];
		filters[CodeType.Image] = [];
		
		return { 
			view: 'browse',
			codeTabView: 'editor',
			index: 0,
			loading: true, 
			loadingImageId: 0,
			data: false,
			filters: filters,
			session: false, 
			account: false,
			scrollTop: 0
		};
	},
	
	componentDidMount: function() {		
		History.Adapter.bind(window, 'statechange', function() {
			if (this.ignoreHistoryStateChange) {
				this.ignoreHistoryStateChange = false;
			} else {
				var hState = History.getState();
				if (!hState.data || !hState.data.view) {
					this.setState({ view: 'browse', index: 0 });
				} else {
					this.setState(hState.data);
				}
			}
		}.bind(this));		
		
		var config = window.ViewerConfig || false,
		    initState = this.getInitialState(),
		    page = location.search,
		    sessionCookie = Cookies.get('mra-session'),
		    session = false;
		
        if (typeof(sessionCookie) !== "undefined") {
        	session = sessionCookie;
            var user = Cookies.get('mra-user');
            try {
            	var acct = JSON.parse(user);
                this.setState({ session: session, account: acct, codeTabView: this.getDefaultTabViewForAccount(acct) });
            } catch(e) {
            	Cookies.remove('mra-session');
                Cookies.remove('mra-user');
            }
        }
		
		if (config) {
			this.csrfToken = config.csrf;
			this.setState({ view: config.view });
			
			if (config.index > 0) {
				var newIndex = config.index - 1;
				initState = { index: newIndex };
				this.setState({ index: newIndex });
			}
			
			else if (config.imageId > 0) {
				this.setState({ loadingImageId: config.imageId });
			}
		}
		
		this.loadData(session, true);
	},
	
	getDefaultTabViewForAccount: function(acct) {
		return (AccountPermission.check(acct, AccountPermission.Edit) ? 'editor' : 'viewer');
	},
	
	loadData: function(session, initialLoad) {
		$.ajax({
            type: 'GET',
            url: '/mra/data',
            data: { 
            	m: this.state.filters[CodeType.Meta].join(), 
            	c: this.state.filters[CodeType.Image].join()
            },
            dataType: 'json',
            cache: false,
            beforeSend : function(xhr) {
            	if (session !== false) {
            		xhr.setRequestHeader("X-Auth-Token", session);
            	}
            }.bind(this),
            success: function(data) {
            	var session = this.state.session,
            	    acct = this.state.account,
            	    index = this.state.index; 
            	
            	if (typeof(data.accounts) === "undefined") {
                	Cookies.remove('mra-session');
                    Cookies.remove('mra-user');
                    session = false;
                    account = false;
            	}
            	
            	if (this.state.loadingImageId !== false) {
            		for (var i = 0; i < data.results.length; i++) {
            			if (data.results[i].id == this.state.loadingImageId) {
            				index = i;
            				break;
            			}
            		}
            	}
            	
            	this.setState({ data: data, index: index, loadingImageId: false, session: session, account: acct });
            	
            	if (initialLoad) {
            		var initState = { view: this.state.view, index: index };
            		History.replaceState(initState, 'MRA Study Viewer', location.search);	
            	}
            	
            	if (this.state.view == "code") {
            		this.fetchImageDetailsByIndex(index, true);
            	} else {
            		this.setState({ loading: false });
            	}
            }.bind(this)
		});
	},
	
    handleLogin: function(form, errorCallback) {
        form[this.csrfToken.name] = this.csrfToken.value;

        $.ajax({
            type: 'POST',
            url: '/account/login',
            data: form,
            dataType: 'json',
            cache: false,
            success: function(results) {
                this.csrfToken = results.csrf;
                if (results.success) {
                    Cookies.set('mra-session', results.token, { expires: 30 });
                    Cookies.set('mra-user', JSON.stringify(results.account), { expires: 30 });
                    
                    this.setState({ 
                    	view: 'code', 
                    	session: results.token, 
                    	account: results.account, 
                    	codeTabView: this.getDefaultTabViewForAccount(results.account),
                    	loading: true 
                    });
                    
                    this.updateHistory('code', this.state.index);
                    this.cache = {};
                    this.loadData(results.token);
                } else {
                    errorCallback();
                }
            }.bind(this),
            error: function() {
                errorCallback();
            }.bind(this)
        });
    },	
	
    handleLogout: function() {
        var data = {};
        data[this.csrfToken.name] = this.csrfToken.value;

        $.ajax({
        	type: 'POST',
            url: '/account/logout',
            data: data,
            dataType: 'json',
            cache: false,
            beforeSend : function(xhr) {
            	xhr.setRequestHeader("X-Auth-Token", this.state.session);
            }.bind(this),
            success: function(results) {
            	this.csrfToken = results.csrf;
            }.bind(this)
        });

        Cookies.remove('mra-session');
        this.setState({ session: false, account: false });
    },	
    
    handleFilters: function(type, newFilters) {
    	var filters = this.state.filters;
    	filters[type] = newFilters; 
    	this.setState({ loading: true, filters: filters, index: 0, scrollTop: 0 });
    	this.loadData(this.state.session, false);
    },
    
    handleRouteChange: function(page, e) {
    	e.preventDefault();
    	
    	if (page == "next") this.handleNext();
    	else if (page == "prev") this.handlePrev();
    	else if (page == "logout") this.handleLogout();
    	else if (page == "code") this.handleSelectIndex(this.state.index);
    	else if (page == "home") {
    		this.setState({ view: 'code', index: 0});
    		this.updateHistory('code', 0);
    	} else {
    		this.setState({ view: page, index: this.state.index });
    		this.updateHistory(page, this.state.index);
    	}
    },
	
	handlePrev: function() {
		var newIndex = this.state.index == 0 ? this.state.data.results.length - 1 : this.state.index - 1;
		this.fetchImageDetailsByIndex(newIndex);
	},
	
	handleNext: function() {
		var newIndex = this.state.index == this.state.data.results.length - 1 ? 0 : this.state.index + 1;
		this.fetchImageDetailsByIndex(newIndex);
	},
	
	handleSelectIndex: function(index) {	
		this.fetchImageDetailsByIndex(index);
	},
	
	handleScroll: function(scrollTop) {
		this.setState({ scrollTop: scrollTop });
	},
	
	fetchImageDetailsByIndex: function(newIndex, completeLoad) {
		var meta = this.state.data.results[newIndex];
		if (typeof(meta) !== "undefined") {
			var imageId = meta.id;
			this.fetchImageDetailsById(imageId, function() {
				this.setState({ view: 'code', index: newIndex });
				this.updateHistory('code', newIndex);
				
	            if (completeLoad) {
	            	this.setState({ loading: false });
	            }
			}.bind(this));
		} 
	},
	
	fetchImageDetailsById: function(imageId, completedCallback) {
		if (typeof(this.cache[imageId]) !== "undefined") {
			completedCallback();
		} else {
			$.ajax({
	            type: 'GET',
	            url: '/image/details',
	            data: { id: imageId },
	            dataType: 'json',
	            cache: false,
	            beforeSend : function(xhr) {
	            	xhr.setRequestHeader("X-Auth-Token", this.state.session);
	            }.bind(this),
	            success: function(results) {
	                if (results.success) {
	                	this.updateCache(imageId, "details", results);
		                completedCallback();
	                } else {
	                	alert('Image details load failed');
	                }
	            }.bind(this),
	            error: function() {
	            	alert('Image details Load failed');
	            }.bind(this)
			});
		}
	},	
	
	updateCodeTab: function(tab) {
		this.setState({ codeTabView: tab });
	},
	
	updateHistory: function(view, index) {
		this.ignoreHistoryStateChange = true;
		
		if (view == 'code') {
			var imageId = this.state.data.results[index].id;
			History.pushState({ view: view, index: index }, 'MRA Study Viewer', '/mra/id/' + imageId);
		} else if (view == 'browse') {
			// filters?
			History.pushState({ view: view, index: index }, 'MRA Study Viewer', '/mra');
		} else {
			History.pushState({ view: view, index: index }, 'MRA Study Viewer', '/mra/' + view);
		}
	},
	
	updateCache: function(imageId, type, data, useAccount) {
		useAccount = useAccount || false;
		if (type == "details") {
			this.cache[imageId] = { thread: data.thread, codes: data.codes, comments: data.comments };
		} else {
			if (typeof(this.cache[imageId]) === "undefined") {
				this.cache[imageId] = {};
			}
			
			if (useAccount) {
				var accountId = this.state.account.id;
				if (typeof(this.cache[imageId][type]) === "undefined") {
					this.cache[imageId][type] = {};
				}
				
				if (typeof(this.cache[imageId][type][accountId]) === "undefined") {
					this.cache[imageId][type][accountId] = {};
				}
				
				this.cache[imageId][type][accountId] = data;
			} else {
				this.cache[imageId][type] = data;
			}
		}
	},
				
    getRoutes: function() {
        return {
            "code": this.renderCodingView,
            "browse": this.renderBrowseView,
            "login": this.renderLogin
        }
    },	
    
    renderBrowseView: function() {
		if (this.state.loading) {
			return (
				<div className="wrapper">
					<Header session={this.state.session} 
		            	 	account={this.state.account} 
		            		view={this.state.view}
		            		filters={this.state.filters} 
            				onRouteChange={this.handleRouteChange} 
		    				onFilterChange={this.handleFilters} />
					<div className="container-fluid">
						<div className="loading-gif"></div>
					</div>
				</div>
			);
		} else {
			return (
				<div className="wrapper">
				    <Header session={this.state.session} 
				            account={this.state.account} 
				            view={this.state.view}
				            filters={this.state.filters} 
		            		onRouteChange={this.handleRouteChange} 
				    		onFilterChange={this.handleFilters}  />
					<div className="container-fluid">
		    			<div className="result-count">{ "found " + this.state.data.results.length + " images"}</div>
						<ImageBrowser data={this.state.data.results} 
									  index={this.state.index}
									  scrollTop={this.state.scrollTop} 
						              onScroll={this.handleScroll} 
						              onImageClick={this.handleSelectIndex} />
					</div>
				</div>
			);
		}
    },
	
	renderCodingView: function() {
		if (this.state.loading) {
			return (
					<div className="wrapper">
						<Header session={this.state.session} 
			            	 	account={this.state.account} 
			            		view={this.state.view}
			            		filters={this.state.filters} 
	            				onRouteChange={this.handleRouteChange} 
			    				onFilterChange={this.handleFilters} />
						<div className="container-fluid">
							<div className="loading-gif"></div>
						</div>
					</div>
				);
		} else {
			var meta = this.state.data.results[this.state.view == 'code' ? this.state.index : this.state.imageId];
			var imgur = "http://imgur.com/" + meta.key;
			var idx = this.state.index + 1;
			var controls = null;
			var mainColStyle = "viewer-column";
			
			var details = false, haveData = false;
			if (typeof(this.cache[meta.id]) !== "undefined") {
				details = this.cache[meta.id];
				haveData = true;
			}
			
			var thread = (haveData) ? details.thread : [];
			
			if (this.state.session !== false) {
				var accountId = this.state.account.id;
				var comments = (haveData) ? details.comments : {};
				var codes = (haveData) ? details.codes : {};
				
				mainColStyle = "col-md-8 viewer-column";
				controls = (
					<div className="col-md-4 viewer-column">
						<CodeContainer 
							view={this.state.codeTabView}
							session={this.state.session}
							account={this.state.account}
							accounts={this.state.data.accounts}
							meta={meta} 
							codes={codes} 
							csrfToken={this.csrfToken} 
							onSelectCodeTab={this.updateCodeTab}
							onUpdate={this.updateCache} />
						
						<div className="left-border">
							<div className="viewer-title">Coder Comments</div>
							<CodeComments meta={meta} 
								  comments={comments} 
					 			  session={this.state.session} 
					              account={this.state.account} 
					              accounts={this.state.data.accounts} 
					              csrfToken={this.csrfToken} 
								  onUpdate={this.updateCache} />
						</div>
					</div>);
			}
			
			return (
				<div className="wrapper">
			    <Header session={this.state.session} 
			            account={this.state.account} 
			            view={this.state.view}
			            filters={this.state.filters}
	            		onRouteChange={this.handleRouteChange}
			    		onFilterChange={this.handleFilters} />
					<div className="container-fluid">
						<div className="row">
							<div className={mainColStyle}>
								<div className="viewer-container">
									<div className="viewer-title">{idx}. {meta.title}</div>
									{ meta.images.map(function(img) {
											var src = "/imgur/" + img.filename;
											return (<a href={imgur} target="_blank"><img className="imgur" src={src} /></a>);
									})}
								</div>
								<div className="viewer-container">
									<div className="comments-title"><a href={meta.url} target="_blank">Reddit Thread</a></div>
									<ThreadViewer meta={meta} thread={thread} onUpdate={this.updateCache} />
								</div>
							</div>
							{controls}
						</div>	
					</div>
				</div>
			);
		}
	},
	
    renderLogin: function() {
        return (
            <div id="wrapper">
                <Header session={this.state.session} account={this.state.account} onroute={this.handleRouteChange} view={this.state.view} />
	            <div className="container-fluid">
	            	<div className="static-content">
	                	<h3>Sign In</h3>
	                	<LoginForm onsubmit={this.handleLogin} />
	                </div>
	            </div>
	        </div>
        );
    },
	
    render: function() {
        var routes = this.getRoutes();
        return routes[this.state.view]();
    }
});
