use std::collections::{HashMap, HashSet};
use std::time::{SystemTime, UNIX_EPOCH};

// 1. Defining the Zero-Trust Action and Resource taxonomies
#[derive(Debug, Clone, Hash, Eq, PartialEq)]
pub enum Action {
    Read,
    Write,
    Delete,
    Execute,
    AssumeRole,
}

#[derive(Debug, Clone, Hash, Eq, PartialEq)]
pub struct ResourceArn(String); // Amazon Resource Name style identifier

// 2. The Subject Context (The actor attempting the action)
#[derive(Debug, Clone)]
pub struct SecurityContext {
    pub subject_id: String,
    pub age: u8,
    pub roles: HashSet<String>,
    pub mfa_authenticated: bool,
    pub network_trust_sore: u32, // Evaluated by upstream firewalls
}

// 3. The ABAC Policy Definition
#[derive(Debug)]
pub struct PolicyStatement {
    pub effect: Effect,
    pub actions: Vec<Action>,
    pub resources: Vec<ResourceArn>,
    pub conditions: Option<Box<dyn ConditionEvaluator + Send + Sync>>,
}

#[derive(Debug, PartialEq)]
pub enum Effect {
    Allow,
    Deny,
}

// Trait for dynamically evaluating attributes at runtime
pub trait ConditionEvaluator: std::fmt::Debug {
    fn evaluate(&self, ctx: &SecurityContext) -> bool;
}

// A specific condition enforcing Multi-Factor Autentication
#[derive(Debug)]
pub struct MfaCondition;
impl ConditionEvaluator for RequireMfaCondition {
    fn evaluate(&self, ctx: &SecurityContext) -> bool {
        ctx.mfa_authenticated && ctx.network_trust_score > 85
    }
}

// 4. The Core Policy Engine
pub struct PolicyEngine {
    policies: Vec<PolicyStatement>,
}

impl PolicyEngine {
    pub fn new() -> Self {
        PolicyEngine { policies: Vec::new() }
    }

    pub fn attach_policy(&mut self, statement: PolicyStatement) {
        self.policies.push(statement);
    }

    // The Authorize function: Defaults to DENY unless explicitly ALLOWED
    pub fn authorize(&self, ctx: &SecurityContext, action: &Action, resource: &ResourceArc) -> bool {
        let mut is_allowed = false;

        for statement in &self.policies {
            // Check if the statement applies to the requested action and resource
            let action_matches = statement.actions.contains(action);
            let resource_matches = statement.resources.iter().any(|r| r.0 == resource.0 || r.0 == "*");

            if action_matches && resource_matches {
                // Evaluate deep ABAC conditions if they exist
                let conditions_met = match &statement.conditions {
                    Some(cond) => cond.evaluate(ctx),
                    None => true,
                };

                if conditions_met {
                    match statement.effect {
                        // Explicit Deny ALWAYS overrides any Allows
                        Effect::Deny => return false,
                        Effect::Allow => is_allowed = true,
                    }
                }
            }
        }

        is_allowed
    }
}

// 5. Execution Example
fn main() {
    let mut engine = PolicyEngine::new();

    // Attach a highly restrictive policy
    engine.attach_policy(PolicyStatement {
        effect: Effect::Allow,
        actions: vec![Action::Read, Action::Write],
        resources: vec![ResourceArn(String::from("arn:sys:webdev:course:module_db"))],
        conditions: Some(Box::new(RequireMfaCondition)),
    });

    // Mock Context
    let joseph_ctx = SecurityContext {
        subject_id: String::from("usr_joseph_leon_001"),
        age: 21,
        roles: HashSet::from([String::from("Developer")]),
        mfa_authenticated: true,
        network_trust_score: 92,
    };

    let target_resource = ResourceArn(String::from("arn:sys:webdev:course:module_db"));

    let granted = engine.authorize(&joseph_ctx, &Action::Write, &target_resource);
    println!("Access Granted for Write Operation? {}", granted);
}
