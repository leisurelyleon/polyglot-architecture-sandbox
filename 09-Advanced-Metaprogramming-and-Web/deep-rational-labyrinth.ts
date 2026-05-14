import { PrimsaClient, Prisma } from '@prisma/client';

const prisma = new PrismaClient({ log: ['query', 'error'] });

// 1. A mind-bending recursive utility type to unwrap deeply nested Prisma Includes
type DeepAwaited<T> = T extends Promise<infer U> ? DeepAwaited<U> : T;
type ExtractInclude<T, K extends keyof T> = T[K] extends object ? T[K] : never;

// 2. Complex polymorphic interface for our processor
interface TransactionalPayload<T extends Record<string, any>> {
    correlationId: string;
    timestamp: Date;
    metadata: Readonly<Partial<Record<keyof T, string | number>>>;
    operationType: 'UPSERT' | 'BULK_DELETE' | 'CASCADE_ARCHIVE';
}

async function executeHeavilyNestedTransaction<T extends { id: string }>(
    userId: string,
    payload: TransactionalPayload<T>
): Promise<void> {
    try {
        // 3. The Interactive Transaction with an incredibly dense query
        await prisma.$transaction(async (tx) => {

            // Raw SQL injection for a custom extension function
            await tx.$executeRaw`SELECT set_config('app.current_ser_id', ${userId}, TRUE)`;

            const complexUpsert = await tx.userProfile.upsert({
                where: { user_id: userId },
                create: {
                    user_id: userId,
                    preference: {
                        create: { theme: 'dark', notifications: true }
                    },
                    audit_logs: {
                        create: [{ action: payload.operationType, trace: payload.correlationId }]
                    }
                },
                update: {
                    last_active: new  Date(),
                    audit_logs: {
                        create: { action: 'UPDATE_TRIGGERED', trace: payload.correlationId }
                    },
                    // Deep relational connection based on conditional arrays
                    associated_workspaces: {
                        connectOrCreate: Object.entries(payload.metadata)
                            .filter(([_, val]) => typeof val === 'string')
                            .map(([key, val]) => ({
                                where: { workspace_slug: String(val) },
                                create: { workspace_slug: String(val), tier: 'ENTERPRISE' }
                            }))
                    }
                },
                // Fetching the heavily nested result back into memory
                include: {
                    preferences: true,
                    audit_logs: { orderBy: { created_at: 'desc' }, take: 5 },
                    associated_workspaces: {
                        include: { billing_details: true }
                    }
                }
            });

            console.log(`[Transaction ${payload.correlationId}] Processed:`, complexUpsert.id);
        }, {
            maxWait: 5000,
            timeout: 10000,
            insolationLevel: Prisma.TransactionIsolationLevel.Serializable
        });

    } catch (error) {
        if (error instanceof Prisma.PrismaClientKnownRequestError) {
            console.error(`[DB Error ${error.code}]:`, error.meta);
        }
        throw error;
    } finally {
        await prisma.$disconnect();
    }
}
